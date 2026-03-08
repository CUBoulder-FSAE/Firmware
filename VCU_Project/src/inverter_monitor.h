#ifndef INVERTER_MONITOR_H
#define INVERTER_MONITOR_H

#include <stdint.h>
#include <stdbool.h>
#include "driverlib.h"
#include "device.h"

//
// ============================================================================
// INVERTER CAN MESSAGE IDs
// ============================================================================
//
#define INVERTER_MSG_ID_0x1F    0x1F    // Control mode, Target Iq, motor position, isMotorStill, ERPM, Duty, Input Voltage
#define INVERTER_MSG_ID_0x20    0x20    // Reserved for future use
#define INVERTER_MSG_ID_0x21    0x21    // AC Current, DC Current
#define INVERTER_MSG_ID_0x22    0x22    // Controller Temp, Motor Temp, Fault code
#define INVERTER_MSG_ID_0x23    0x23    // Id, Iq values
#define INVERTER_MSG_ID_0x24    0x24    // Throttle signal, Brake signal, Digital I/Os, Drive enable, Limit status bits, CAN map version
#define INVERTER_MSG_ID_0x25    0x25    // AC Current limits (configured max/min, available max/min)
#define INVERTER_MSG_ID_0x26    0x26    // DC Current limits (configured max/min, available max/min)

//
// ============================================================================
// INVERTER DATA STRUCTURE
// ============================================================================
//
// Current limit structure
typedef struct {
    float configuredMax;            // Configured maximum current (A)
    float configuredMin;            // Configured minimum current (A)
    float availableMax;             // Available maximum current (A)
    float availableMin;             // Available minimum current (A)
} current_limit_t;

typedef struct {
    // Message 0x1F data - Control & Status
    uint8_t controlMode;            // Control mode enumeration (0-7)
    float targetIq;                 // Target Iq current (A, scale 10)
    float motorPosition;            // Motor position (degrees, scale 10, range 0-359)
    uint8_t isMotorStill;           // Flag: motor is still (0=moving, 1=still)
    // Bytes 6-7: Reserved
    
    // Message 0x20 data - General data 1
    uint32_t erpm;                  // Electrical RPM (no scale) - Motor RPM * pole pairs
    float dutyCycle;                // PWM duty cycle percentage (%, scale 10, range -100 to 100)
    float inputVoltage;             // DC Input voltage (V, scale 1)
    
    // Message 0x21 data - General data 2
    float acCurrent;                // AC RMS current (A, scale 10)
    float dcCurrent;                // DC current (A, scale 10)
    
    // Message 0x22 data - General data 3
    float controllerTemp;           // Controller temperature (°C, scale 10, range -40 to 125)
    float motorTemp;                // Motor temperature (°C, scale 10, range -40 to 125)
    uint16_t faultCode;             // Fault code from inverter (no scale)
    
    // Message 0x23 data - General data 4
    float idCurrent;                // Id current (A, scale 10)
    float iqCurrent;                // Iq current (A, scale 10)
    
    // Message 0x24 data - General data 5
    float throttleSignal;           // Throttle signal (%, scale 10, range 0-100)
    float brakeSignal;              // Brake signal (%, scale 10, range 0-100)
    uint8_t digitalIOs;             // Digital I/O states (8 bits)
    uint8_t driveEnable;            // Drive enable flag (bit 0)
    uint8_t limitStatus;            // Limit status bits (bits 1-7)
    uint8_t canMapVersion;          // CAN message mapping version
    
    // Message 0x25 data - General data 6 (AC current limits)
    current_limit_t acCurrentLimits;    // AC current limits (A, scale 10)
    
    // Message 0x26 data - General data 7 (DC current limits)
    current_limit_t dcCurrentLimits;    // DC current limits (A, scale 10)
    
    // Status tracking
    uint32_t lastUpdateTime;            // Timestamp of last update (ticks)
    uint32_t messagesReceived;          // Count of messages received
    uint32_t parseErrors;               // Count of parse errors
} inverter_data_t;

//
// ============================================================================
// INVERTER TASK STATISTICS
// ============================================================================
//
typedef struct {
    uint32_t messagesProcessed;     // Total messages processed from queue
    uint32_t messagesDropped;       // Messages dropped due to queue full
    uint32_t unknownMessageIds;     // Count of unknown message IDs
    uint32_t parseErrors;           // Parse or validation errors
    uint32_t queueReceiveTimeouts;  // Times queue receive timed out
} inverter_stats_t;

//
// ============================================================================
// PUBLIC API FUNCTIONS
// ============================================================================
//

/**
 * inverter_task_init - Initialize inverter monitoring task and create queue
 * 
 * This function must be called once during system initialization.
 * It creates the FreeRTOS task and the inter-task queue for CAN messages.
 */
void inverter_task_init(void);

/**
 * inverter_task_loop - Main task loop for inverter message processing
 * 
 * Parameters:
 *   parameters - FreeRTOS task parameter (typically NULL)
 * 
 * This is the main inverter task entry point. It reads CAN messages from
 * a queue (fed by can_task) and parses inverter-specific message IDs.
 */
void inverter_task_loop(void* parameters);

/**
 * inverter_get_data - Get current inverter state data (thread-safe copy)
 * 
 * Parameters:
 *   data - Pointer to inverter_data_t structure to fill
 * 
 * Returns:
 *   1 if successful, 0 if error
 * 
 * Example:
 *   inverter_data_t invData;
 *   if(inverter_get_data(&invData))
 *   {
 *       uint16_t rpm = invData.erpm;
 *       int16_t temp = invData.motorTemp;
 *   }
 */
uint8_t inverter_get_data(inverter_data_t* data);

/**
 * inverter_get_stats - Get inverter task statistics
 * 
 * Parameters:
 *   stats - Pointer to inverter_stats_t structure to fill
 * 
 * Returns:
 *   1 if successful, 0 if error
 */
uint8_t inverter_get_stats(inverter_stats_t* stats);

/**
 * inverter_is_motor_spinning - Check if motor is currently spinning
 * 
 * Returns:
 *   1 if motor is spinning (ERPM > threshold), 0 if still or unknown
 */
uint8_t inverter_is_motor_spinning(void);

/**
 * inverter_get_rpm - Get current motor RPM
 * 
 * Returns:
 *   Current ERPM value from last received message
 */
uint16_t inverter_get_rpm(void);

/**
 * inverter_get_motor_temp - Get current motor temperature
 * 
 * Returns:
 *   Current motor temperature from last received message
 */
int16_t inverter_get_motor_temp(void);

/**
 * inverter_has_fault - Check if inverter has reported a fault
 * 
 * Returns:
 *   1 if fault code is non-zero, 0 if no fault
 */
uint8_t inverter_has_fault(void);

/**
 * inverter_get_fault_code - Get current fault code
 * 
 * Returns:
 *   Current fault code from last received message
 */
uint16_t inverter_get_fault_code(void);

#endif  // INVERTER_MONITOR_H