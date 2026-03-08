//
// FILE: inverter_monitor.c
//
// DESCRIPTION: Inverter monitoring task - reads CAN messages from queue
//              and parses inverter-specific data (RPM, temperatures, currents, etc.)
//
// This module provides:
// - FreeRTOS task for inverter status monitoring
// - Queue-based message reception from can_task
// - Message ID filtering and parsing
// - Thread-safe access to inverter state
// - Statistics tracking for debugging
//

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "inverter_monitor.h"
#include "can_task.h"

//
// ============================================================================
// TASK CONFIGURATION
// ============================================================================
//

#define INVERTER_TASK_STACK_SIZE    512
#define INVERTER_TASK_PRIORITY      3           // Higher priority than can_task (2)
#define INVERTER_QUEUE_TIMEOUT_MS   10          // Timeout for queue receive

// RPM threshold to consider motor spinning
#define INVERTER_MIN_RPM_THRESHOLD  100

//
// ============================================================================
// INTERNAL DATA STRUCTURES
// ============================================================================
//

// CAN Message wrapper (must match can_task.c definition)
typedef struct {
    uint32_t id;                // Message ID
    uint8_t dlc;                // Data Length Code (0-8)
    uint8_t data[8];            // Message data bytes
    uint32_t timestamp;         // Timestamp of message
} CAN_Message_t;

//
// ============================================================================
// GLOBAL VARIABLES
// ============================================================================
//

static TaskHandle_t inverter_task_handle = NULL;

// Inverter state data (protected by critical section when accessed from multiple tasks)
static inverter_data_t inverter_data = {0};

// Statistics
static inverter_stats_t inverter_stats = {0};

//
// ============================================================================
// INTERNAL HELPER FUNCTIONS
// ============================================================================
//

/**
 * parse_message_0x1F - Parse message 0x1F
 * Control mode, Target Iq, Motor position, isMotorStill (Bytes 6-7 Reserved)
 * 
 * Byte layout:
 *   Byte 0: Control mode (0-7)
 *   Bytes 1-2: Target Iq (int16_t, big-endian, scale 10, range -3276.8 to 3276.7 A)
 *   Bytes 3-4: Motor position (int16_t, big-endian, scale 10, range 0-359 degrees)
 *   Byte 5: isMotorStill (0=not still, 1=still)
 *   Bytes 6-7: Reserved (fill with FFs)
 */
static void parse_message_0x1F(const CAN_Message_t* msg) {
    if (msg->dlc < 8) {
        inverter_data.parseErrors++;
        return;
    }
    
    // Byte 0: Control mode
    inverter_data.controlMode = msg->data[0];
    
    // Bytes 1-2: Target Iq (int16_t, big-endian, scale 10)
    int16_t targetIq_raw = ((int16_t)msg->data[1] << 8) | msg->data[2];
    inverter_data.targetIq = targetIq_raw / 10.0f;
    
    // Bytes 3-4: Motor position (int16_t, big-endian, scale 10)
    int16_t motorPos_raw = ((int16_t)msg->data[3] << 8) | msg->data[4];
    inverter_data.motorPosition = motorPos_raw / 10.0f;
    
    // Byte 5: isMotorStill
    inverter_data.isMotorStill = (msg->data[5] & 0x01);
    
    // Bytes 6-7: Reserved - no parsing
}

/**
 * parse_message_0x20 - Parse message 0x20
 * General data 1 (ERPM, Duty cycle, Input voltage)
 * 
 * Byte layout:
 *   Bytes 0-3: ERPM (uint32_t, big-endian, no scale, range 0-214748364)
 *             Equation: ERPM = Motor RPM * number of motor pole pairs
 *   Bytes 4-5: Duty cycle (int16_t, big-endian, scale 10, range -100 to 100%)
 *             Sign represents motor running(positive) or regenerating(negative)
 *   Bytes 6-7: Input voltage (int16_t, big-endian, scale 1, range -32768 to 32767 V)
 *             Input voltage is the DC voltage
 */
static void parse_message_0x20(const CAN_Message_t* msg) {
    if (msg->dlc < 8) {
        inverter_data.parseErrors++;
        return;
    }
    
    // Bytes 0-3: ERPM (uint32_t, big-endian, no scaling)
    inverter_data.erpm = ((uint32_t)msg->data[0] << 24) | 
                         ((uint32_t)msg->data[1] << 16) | 
                         ((uint32_t)msg->data[2] << 8) | 
                         msg->data[3];
    
    // Bytes 4-5: Duty cycle (int16_t, big-endian, scale 10)
    // Positive = motor running, Negative = regenerating
    int16_t dutyCycle_raw = ((int16_t)msg->data[4] << 8) | msg->data[5];
    inverter_data.dutyCycle = dutyCycle_raw / 10.0f;
    
    // Bytes 6-7: Input voltage (int16_t, big-endian, scale 1)
    int16_t inputVoltage_raw = ((int16_t)msg->data[6] << 8) | msg->data[7];
    inverter_data.inputVoltage = (float)inputVoltage_raw;
}

/**
 * parse_message_0x21 - Parse message 0x21
 * General data 2 (AC Current, DC Current)
 * 
 * Byte layout:
 *   Bytes 0-1: AC RMS current (int16_t, big-endian, scale 10, range -3276.8 to 3276.7 A)
 *   Bytes 2-3: DC current (int16_t, big-endian, scale 10, range -3276.8 to 3276.7 A)
 *   Bytes 4-7: Reserved
 */
static void parse_message_0x21(const CAN_Message_t* msg) {
    if (msg->dlc < 4) {
        inverter_data.parseErrors++;
        return;
    }
    
    // Bytes 0-1: AC RMS current (int16_t, big-endian, scale 10)
    int16_t acCurrent_raw = ((int16_t)msg->data[0] << 8) | msg->data[1];
    inverter_data.acCurrent = acCurrent_raw / 10.0f;
    
    // Bytes 2-3: DC current (int16_t, big-endian, scale 10)
    int16_t dcCurrent_raw = ((int16_t)msg->data[2] << 8) | msg->data[3];
    inverter_data.dcCurrent = dcCurrent_raw / 10.0f;
    
    // Bytes 4-7: Reserved - no parsing
}

/**
 * parse_message_0x22 - Parse message 0x22
 * General data 3 (Controller Temp, Motor Temp, Fault code)
 * 
 * Byte layout:
 *   Bytes 0-1: Controller temperature (int16_t, big-endian, scale 10, range -40 to 125 °C)
 *   Bytes 2-3: Motor temperature (int16_t, big-endian, scale 10, range -40 to 125 °C)
 *   Bytes 4-5: Fault code (uint16_t, big-endian, no scale)
 *   Bytes 6-7: Reserved
 */
static void parse_message_0x22(const CAN_Message_t* msg) {
    if (msg->dlc < 6) {
        inverter_data.parseErrors++;
        return;
    }
    
    // Bytes 0-1: Controller temperature (int16_t, big-endian, scale 10)
    int16_t ctrlTemp_raw = ((int16_t)msg->data[0] << 8) | msg->data[1];
    inverter_data.controllerTemp = ctrlTemp_raw / 10.0f;
    
    // Bytes 2-3: Motor temperature (int16_t, big-endian, scale 10)
    int16_t motorTemp_raw = ((int16_t)msg->data[2] << 8) | msg->data[3];
    inverter_data.motorTemp = motorTemp_raw / 10.0f;
    
    // Bytes 4-5: Fault code (uint16_t, big-endian, no scale)
    inverter_data.faultCode = ((uint16_t)msg->data[4] << 8) | msg->data[5];
    
    // Bytes 6-7: Reserved - no parsing
}

/**
 * parse_message_0x23 - Parse message 0x23
 * General data 4 (Id, Iq values)
 * 
 * Byte layout:
 *   Bytes 0-1: Id current (int16_t, big-endian, scale 10, range -3276.8 to 3276.7 A)
 *   Bytes 2-3: Iq current (int16_t, big-endian, scale 10, range -3276.8 to 3276.7 A)
 *   Bytes 4-7: Reserved
 */
static void parse_message_0x23(const CAN_Message_t* msg) {
    if (msg->dlc < 4) {
        inverter_data.parseErrors++;
        return;
    }
    
    // Bytes 0-1: Id current (int16_t, big-endian, scale 10)
    int16_t idCurrent_raw = ((int16_t)msg->data[0] << 8) | msg->data[1];
    inverter_data.idCurrent = idCurrent_raw / 10.0f;
    
    // Bytes 2-3: Iq current (int16_t, big-endian, scale 10)
    int16_t iqCurrent_raw = ((int16_t)msg->data[2] << 8) | msg->data[3];
    inverter_data.iqCurrent = iqCurrent_raw / 10.0f;
    
    // Bytes 4-7: Reserved - no parsing
}

/**
 * parse_message_0x24 - Parse message 0x24
 * General data 5 (Throttle, Brake, Digital I/Os, Drive enable, Limit status, CAN map version)
 * 
 * Byte layout:
 *   Bytes 0-1: Throttle signal (int16_t, big-endian, scale 10, range 0-100%)
 *   Bytes 2-3: Brake signal (int16_t, big-endian, scale 10, range 0-100%)
 *   Byte 4: Digital I/Os (8 bits)
 *   Byte 5: Drive enable (bit 0) + Limit status bits (bits 1-7)
 *   Byte 6: CAN map version
 *   Byte 7: Reserved
 */
static void parse_message_0x24(const CAN_Message_t* msg) {
    if (msg->dlc < 7) {
        inverter_data.parseErrors++;
        return;
    }
    
    // Bytes 0-1: Throttle signal (int16_t, big-endian, scale 10)
    int16_t throttle_raw = ((int16_t)msg->data[0] << 8) | msg->data[1];
    inverter_data.throttleSignal = throttle_raw / 10.0f;
    
    // Bytes 2-3: Brake signal (int16_t, big-endian, scale 10)
    int16_t brake_raw = ((int16_t)msg->data[2] << 8) | msg->data[3];
    inverter_data.brakeSignal = brake_raw / 10.0f;
    
    // Byte 4: Digital I/Os (bit flags)
    inverter_data.digitalIOs = msg->data[4];
    
    // Byte 5: Drive enable + Limit status
    inverter_data.driveEnable = (msg->data[5] & 0x01);
    inverter_data.limitStatus = (msg->data[5] >> 1) & 0x7F;
    
    // Byte 6: CAN map version
    inverter_data.canMapVersion = msg->data[6];
    
    // Byte 7: Reserved - no parsing
}

/**
 * parse_message_0x25 - Parse message 0x25
 * General data 6 (AC Current limits)
 * 
 * Byte layout:
 *   Bytes 0-1: Configured AC max current (int16_t, big-endian, scale 10, range -3276.8 to 3276.7 A)
 *   Bytes 2-3: Configured AC min current (int16_t, big-endian, scale 10, range -3276.8 to 3276.7 A)
 *   Bytes 4-5: Available AC max current (int16_t, big-endian, scale 10, range -3276.8 to 3276.7 A)
 *   Bytes 6-7: Available AC min current (int16_t, big-endian, scale 10, range -3276.8 to 3276.7 A)
 */
static void parse_message_0x25(const CAN_Message_t* msg) {
    if (msg->dlc < 8) {
        inverter_data.parseErrors++;
        return;
    }
    
    // Bytes 0-1: Configured AC max current (int16_t, big-endian, scale 10)
    int16_t acMaxCfg_raw = ((int16_t)msg->data[0] << 8) | msg->data[1];
    inverter_data.acCurrentLimits.configuredMax = acMaxCfg_raw / 10.0f;
    
    // Bytes 2-3: Configured AC min current (int16_t, big-endian, scale 10)
    int16_t acMinCfg_raw = ((int16_t)msg->data[2] << 8) | msg->data[3];
    inverter_data.acCurrentLimits.configuredMin = acMinCfg_raw / 10.0f;
    
    // Bytes 4-5: Available AC max current (int16_t, big-endian, scale 10)
    int16_t acMaxAvail_raw = ((int16_t)msg->data[4] << 8) | msg->data[5];
    inverter_data.acCurrentLimits.availableMax = acMaxAvail_raw / 10.0f;
    
    // Bytes 6-7: Available AC min current (int16_t, big-endian, scale 10)
    int16_t acMinAvail_raw = ((int16_t)msg->data[6] << 8) | msg->data[7];
    inverter_data.acCurrentLimits.availableMin = acMinAvail_raw / 10.0f;
}

/**
 * parse_message_0x26 - Parse message 0x26
 * General data 7 (DC Current limits)
 * 
 * Byte layout:
 *   Bytes 0-1: Configured DC max current (int16_t, big-endian, scale 10, range -3276.8 to 3276.7 A)
 *   Bytes 2-3: Configured DC min current (int16_t, big-endian, scale 10, range -3276.8 to 3276.7 A)
 *   Bytes 4-5: Available DC max current (int16_t, big-endian, scale 10, range -3276.8 to 3276.7 A)
 *   Bytes 6-7: Available DC min current (int16_t, big-endian, scale 10, range -3276.8 to 3276.7 A)
 */
static void parse_message_0x26(const CAN_Message_t* msg) {
    if (msg->dlc < 8) {
        inverter_data.parseErrors++;
        return;
    }
    
    // Bytes 0-1: Configured DC max current (int16_t, big-endian, scale 10)
    int16_t dcMaxCfg_raw = ((int16_t)msg->data[0] << 8) | msg->data[1];
    inverter_data.dcCurrentLimits.configuredMax = dcMaxCfg_raw / 10.0f;
    
    // Bytes 2-3: Configured DC min current (int16_t, big-endian, scale 10)
    int16_t dcMinCfg_raw = ((int16_t)msg->data[2] << 8) | msg->data[3];
    inverter_data.dcCurrentLimits.configuredMin = dcMinCfg_raw / 10.0f;
    
    // Bytes 4-5: Available DC max current (int16_t, big-endian, scale 10)
    int16_t dcMaxAvail_raw = ((int16_t)msg->data[4] << 8) | msg->data[5];
    inverter_data.dcCurrentLimits.availableMax = dcMaxAvail_raw / 10.0f;
    
    // Bytes 6-7: Available DC min current (int16_t, big-endian, scale 10)
    int16_t dcMinAvail_raw = ((int16_t)msg->data[6] << 8) | msg->data[7];
    inverter_data.dcCurrentLimits.availableMin = dcMinAvail_raw / 10.0f;
}

/**
 * process_message - Process a CAN message and update inverter state
 * 
 * Parameters:
 *   msg - Pointer to received CAN message
 */
static void process_message(const CAN_Message_t* msg) {
    switch (msg->id) {
        case INVERTER_MSG_ID_0x1F:
            parse_message_0x1F(msg);
            break;
        case INVERTER_MSG_ID_0x20:
            parse_message_0x20(msg);
            break;
        case INVERTER_MSG_ID_0x21:
            parse_message_0x21(msg);
            break;
        case INVERTER_MSG_ID_0x22:
            parse_message_0x22(msg);
            break;
        case INVERTER_MSG_ID_0x23:
            parse_message_0x23(msg);
            break;
        case INVERTER_MSG_ID_0x24:
            parse_message_0x24(msg);
            break;
        case INVERTER_MSG_ID_0x25:
            parse_message_0x25(msg);
            break;
        case INVERTER_MSG_ID_0x26:
            parse_message_0x26(msg);
            break;
        default:
            // Unknown message ID
            inverter_stats.unknownMessageIds++;
            break;
    }
    
    // Update timestamp and message counter
    inverter_data.lastUpdateTime = xTaskGetTickCount();
    inverter_data.messagesReceived++;
    inverter_stats.messagesProcessed++;
}

//
// ============================================================================
// PUBLIC API IMPLEMENTATION
// ============================================================================
//

void inverter_task_init(void) {
    // Get the CAN message queue from can_task
    QueueHandle_t can_queue = can_get_message_queue();
    
    if (can_queue == NULL) {
        // CAN task not initialized - cannot proceed
        // TODO: Log error
        return;
    }
    
    // Create inverter monitoring task
    BaseType_t status = xTaskCreate(
        inverter_task_loop,
        "Inverter Task",
        INVERTER_TASK_STACK_SIZE,
        (void*)can_queue,            // Pass queue handle as parameter
        INVERTER_TASK_PRIORITY,
        &inverter_task_handle
    );

    if (status != pdPASS) {
        // TODO: Log task creation failure
        inverter_task_handle = NULL;
    }
}

void inverter_task_loop(void* parameters) {
    QueueHandle_t can_queue = (QueueHandle_t)parameters;
    CAN_Message_t rxMsg;
    BaseType_t qStatus;
    
    if (can_queue == NULL) {
        // Invalid queue - task cannot function
        vTaskDelete(NULL);
        return;
    }
    
    // Main task loop
    while (1) {
        // Wait for a message from the CAN queue
        qStatus = xQueueReceive(
            can_queue,
            &rxMsg,
            pdMS_TO_TICKS(INVERTER_QUEUE_TIMEOUT_MS)
        );
        
        if (qStatus == pdPASS) {
            // Message received - process it
            taskENTER_CRITICAL();
            process_message(&rxMsg);
            taskEXIT_CRITICAL();
        } else {
            // Queue timeout - no message available
            inverter_stats.queueReceiveTimeouts++;
            
            // Continue polling; task can be used for periodic checks if needed
        }
    }
}

uint8_t inverter_get_data(inverter_data_t* data) {
    if (data == NULL) {
        return 0U;
    }
    
    // Copy inverter data (protected by critical section)
    taskENTER_CRITICAL();
    memcpy(data, &inverter_data, sizeof(inverter_data_t));
    taskEXIT_CRITICAL();
    
    return 1U;
}

uint8_t inverter_get_stats(inverter_stats_t* stats) {
    if (stats == NULL) {
        return 0U;
    }
    
    // Copy statistics (protected by critical section)
    taskENTER_CRITICAL();
    memcpy(stats, &inverter_stats, sizeof(inverter_stats_t));
    taskEXIT_CRITICAL();
    
    return 1U;
}

uint8_t inverter_is_motor_spinning(void) {
    uint8_t spinning;
    
    taskENTER_CRITICAL();
    spinning = (inverter_data.erpm > INVERTER_MIN_RPM_THRESHOLD) ? 1U : 0U;
    taskEXIT_CRITICAL();
    
    return spinning;
}

uint16_t inverter_get_rpm(void) {
    uint16_t rpm;
    
    taskENTER_CRITICAL();
    rpm = inverter_data.erpm;
    taskEXIT_CRITICAL();
    
    return rpm;
}

int16_t inverter_get_motor_temp(void) {
    int16_t temp;
    
    taskENTER_CRITICAL();
    temp = inverter_data.motorTemp;
    taskEXIT_CRITICAL();
    
    return temp;
}

uint8_t inverter_has_fault(void) {
    uint8_t hasFault;
    
    taskENTER_CRITICAL();
    hasFault = (inverter_data.faultCode != 0U) ? 1U : 0U;
    taskEXIT_CRITICAL();
    
    return hasFault;
}

uint16_t inverter_get_fault_code(void) {
    uint16_t faultCode;
    
    taskENTER_CRITICAL();
    faultCode = inverter_data.faultCode;
    taskEXIT_CRITICAL();
    
    return faultCode;
}

//
// End of File
//