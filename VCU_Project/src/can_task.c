#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "can_task.h"
#include "inverter_monitor.h"
#include "driverlib.h"
#include "device.h"


#define CAN_TASK_STACK_SIZE     512
#define CAN_TASK_PRIORITY       2
#define CAN_TASK_DELAY_MS       100         // Task monitors status, not message reception

#define CAN_MESSAGE_QUEUE_SIZE  16

// Single RX message object number for receiving all messages
#define CAN_RX_MSG_OBJ          1

// Accept all standard CAN IDs (11-bit, mask = 0x000)
#define CAN_RX_MSG_ID           0x000
#define CAN_RX_MSG_ID_MASK      0x000

// CAN Message Structure (must match inverter_monitor.h expectations)
typedef struct {
    uint32_t id;                // Message ID
    uint8_t dlc;                // Data Length Code (0-8)
    uint8_t data[8];            // Message data bytes
    uint32_t timestamp;         // Timestamp of message
} CAN_Message_t;

static TaskHandle_t can_task_handle;
static QueueHandle_t can_message_queue = NULL;

// ISR statistics
static volatile uint32_t can_messages_in_isr = 0;
static volatile uint32_t can_isr_queue_drops = 0;

//
// ============================================================================
// CAN INTERRUPT SERVICE ROUTINE
// ============================================================================
//

//
// CANA0_ISR - CAN-A Interrupt Line 0 Handler
//
// This ISR is triggered when a CAN message is received on message object 1.
// It reads the message using CAN_readMessageWithID() and sends it to the
// queue for processing by the inverter_task.
//
__interrupt void CANA0_ISR(void) {
    CAN_Message_t rxMsg;
    uint32_t intCause;
    uint32_t msgId = 0;
    uint16_t msgDataRaw[4];  // CAN reads as 16-bit words (8 bytes = 4 words)
    uint32_t dlcVal;
    int i;
    
    // Get which message object triggered the interrupt
    intCause = CAN_getInterruptCause(CANA_BASE);
    
    // Check if this is our RX message object
    if (intCause == CAN_RX_MSG_OBJ) {
        
        // Read the message with ID using driverlib function
        bool msgValid = CAN_readMessageWithID(CANA_BASE, CAN_RX_MSG_OBJ, &msgId, msgDataRaw);
        
        if (msgValid) {
            // Get DLC from the message control register
            // The IF1MCTL register contains DLC in bits [3:0]
            dlcVal = (HWREGH(CANA_BASE + CAN_O_IF1MCTL) & CAN_IF1MCTL_DLC_M);
            
            // Limit DLC to 8 bytes
            if (dlcVal > 8U) {
                dlcVal = 8U;
            }
            
            // Populate the message structure
            rxMsg.id = msgId;
            rxMsg.dlc = (uint8_t)dlcVal;
            rxMsg.timestamp = xTaskGetTickCount();
            
            // Convert message data from uint16_t array to uint8_t array
            // CAN driverlib uses 16-bit accesses, so we need to extract bytes
            for (i = 0; i < (int)rxMsg.dlc; i++) {
                // Each uint16_t word contains 2 bytes
                uint16_t word = msgDataRaw[i / 2];
                if (i % 2 == 0) {
                    // Low byte of this word
                    rxMsg.data[i] = (uint8_t)(word & 0xFFU);
                } else {
                    // High byte of this word
                    rxMsg.data[i] = (uint8_t)((word >> 8) & 0xFFU);
                }
            }
            
            // Pad remaining bytes with zeros
            for (i = rxMsg.dlc; i < 8; i++) {
                rxMsg.data[i] = 0U;
            }
            
            // Send message to queue using ISR-safe function
            if (can_message_queue != NULL) {
                BaseType_t qStatus = xQueueSendFromISR(can_message_queue, &rxMsg, NULL);
                if (qStatus != pdPASS) {
                    // Queue is full - message dropped
                    can_isr_queue_drops++;
                } else {
                    can_messages_in_isr++;
                }
            }
        }
    }
    
    // Clear the interrupt in CAN controller
    CAN_clearInterruptStatus(CANA_BASE, intCause);
    
    // Acknowledge this ISR to PIE (Peripheral Interrupt Expansion)
    // CANA0 is group 9, number 5
    Interrupt_clearACKbit(INTERRUPT_ACK_GROUP9);
}

//
// ============================================================================
// TASK INITIALIZATION AND LOOP
// ============================================================================
//

void can_task_init(void) {
    // Create message queue for inter-task communication with ISR
    can_message_queue = xQueueCreate(CAN_MESSAGE_QUEUE_SIZE, sizeof(CAN_Message_t));
    
    if (can_message_queue == NULL) {
        // TODO: Log queue creation failure
        return;
    }
    
    // =========================================================================
    // CAN HARDWARE INITIALIZATION
    // =========================================================================
    
    // Enable CAN module in system control
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_CANA);
    
    // Initialize CAN module (puts it in init state)
    CAN_initModule(CANA_BASE);
    
    // Initialize CAN RAM
    CAN_initRAM(CANA_BASE);
    
    // Configure CAN bit rate
    // Parameters: base, clock frequency, bit rate, bit time
    // 500 kbps is typical for vehicle applications
    CAN_setBitRate(CANA_BASE, DEVICE_SYSCLK_FREQ, 500000U, 16U);
    
    // Enable CAN module
    CAN_enableModule(CANA_BASE);
    
    // =========================================================================
    // CAN MESSAGE OBJECT CONFIGURATION
    // =========================================================================
    
    // Configure RX message object to accept all standard CAN IDs
    CAN_setupMessageObject(
        CANA_BASE,                          // CAN base address
        CAN_RX_MSG_OBJ,                     // Message object number (1)
        CAN_RX_MSG_ID,                      // Message ID to match (0x000 = match all)
        CAN_MSG_FRAME_STD,                  // Standard CAN frame (11-bit ID)
        CAN_MSG_OBJ_TYPE_RX,                // Configure as receive object
        CAN_RX_MSG_ID_MASK,                 // ID mask (0x000 = accept all)
        CAN_MSG_OBJ_RX_INT_ENABLE,          // Enable interrupt on message reception
        8U                                  // Data length
    );
    
    // =========================================================================
    // CAN INTERRUPT CONFIGURATION
    // =========================================================================
    
    // Enable CAN controller interrupt sources
    // CAN_INT_IE0: Interrupt line 0 (message object interrupts)
    // CAN_INT_ERROR: Error interrupt
    // CAN_INT_STATUS: Status change interrupt
    CAN_enableInterrupt(CANA_BASE, CAN_INT_IE0 | CAN_INT_ERROR | CAN_INT_STATUS);
    
    // Register CAN interrupt handler with PIE vector table
    // INT_CANA0 = PIE group 9, interrupt 5 (9.5)
    Interrupt_register(INT_CANA0, &CANA0_ISR);
    
    // Enable the CAN interrupt at PIE level
    Interrupt_enable(INT_CANA0);
    
    // =========================================================================
    // FREERTOS TASK CREATION
    // =========================================================================
    
    // Create CAN monitoring task
    // With ISR-driven reception, this task mainly monitors bus status
    BaseType_t status = xTaskCreate(
        can_task_loop,
        "Can Task",
        CAN_TASK_STACK_SIZE,
        NULL,
        CAN_TASK_PRIORITY,
        &can_task_handle
    );

    if (status != pdPASS) {
        // TODO: Log task creation failure
        vQueueDelete(can_message_queue);
        can_message_queue = NULL;
    }
}

void can_task_loop(void* parameters) {
    TickType_t last_wake_time = xTaskGetTickCount();
    uint32_t canStatus;
    
    // Task runs periodically to monitor CAN bus status and error conditions
    // Actual message reception is handled entirely by the ISR (CANA0_ISR)
    
    while (1) {
        // Read CAN status register to check for error conditions
        canStatus = CAN_getStatus(CANA_BASE);
        
        // Check for critical error conditions
        if (canStatus & CAN_STATUS_BUS_OFF) {
            // Bus off condition detected
            // TODO: Implement recovery strategy
            //       - Log error
            //       - Reset CAN module
            //       - Attempt reinitialize after delay
        }
        
        if (canStatus & CAN_STATUS_EWARN) {
            // Error warning threshold reached (error counter >= 96)
            // TODO: Log warning
        }
        
        if (canStatus & CAN_STATUS_EPASS) {
            // Error passive state (error counter >= 128)
            // TODO: Log passive state
        }
        
        // Periodic task delay - ISR handles all message reception
        xTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(CAN_TASK_DELAY_MS));
    }
}

//
// can_get_message_queue - Get the CAN message queue handle
//
// Returns:
//   Handle to the CAN message queue, or NULL if not initialized
//
QueueHandle_t can_get_message_queue(void) {
    return can_message_queue;
}

//
// can_get_isr_stats - Get ISR statistics for debugging
//
// Parameters:
//   messages - Pointer to store message count from ISR
//   drops - Pointer to store queue drop count
//
void can_get_isr_stats(uint32_t* messages, uint32_t* drops) {
    if (messages != NULL) {
        *messages = can_messages_in_isr;
    }
    if (drops != NULL) {
        *drops = can_isr_queue_drops;
    }
}
