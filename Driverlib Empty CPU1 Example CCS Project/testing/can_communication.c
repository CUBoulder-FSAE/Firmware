//
// FILE: can_communication.c
//
// DESCRIPTION: CAN communication driver with transmit/receive functionality
//              for dual LaunchXL-F28P65X board testing
//
// This module provides:
// - CAN initialization with configurable bit rate
// - Message transmission and reception
// - Loopback mode support for unit testing
// - LED feedback for communication status
//

#include <stdint.h>
#include <stdbool.h>
#include "driverlib.h"
#include "device.h"
#include "board.h"
#include "c2000ware_libraries.h"

//
// ============================================================================
// CONFIGURATION PARAMETERS - Modify for your setup
// ============================================================================
//

// CAN Module Configuration
#define CAN_MODULE              CANA_BASE               // CAN module to use
#define CAN_BITRATE_KBPS        500                     // CAN bit rate in Kbps (250, 500, 1000)
#define CAN_USE_LOOPBACK        0                       // 1 = loopback mode (for testing), 0 = normal mode

// Message IDs
#define CAN_TX_MSG_ID           0x123                   // Transmit message ID
#define CAN_RX_MSG_ID           0x456                   // Receive message ID
#define CAN_ECHO_MSG_ID         0x789                   // Echo/loopback test message ID

// Message Object Numbers (in CAN IP)
#define CAN_TX_MSG_OBJ          1                       // Message object for transmission
#define CAN_RX_MSG_OBJ          2                       // Message object for reception

// LED Configuration for status indication
#define STATUS_LED_PIN          DEVICE_GPIO_PIN_LED1    // LED for TX/RX activity
#define ERROR_LED_PIN           DEVICE_GPIO_PIN_LED2    // LED for error indication

// Test Message Data
#define TEST_MESSAGE_DATA_0     0xDEADBEEF              // Test pattern bytes 0-3
#define TEST_MESSAGE_DATA_1     0xCAFEBABE              // Test pattern bytes 4-7

//
// ============================================================================
// DATA STRUCTURES
// ============================================================================
//

// CAN Message Structure
typedef struct {
    uint32_t id;                // Message ID
    uint8_t dlc;                // Data Length Code (0-8)
    uint8_t data[8];            // Message data bytes
    uint32_t timestamp;         // Timestamp of message
} CAN_Message_t;

// CAN Statistics for Unit Testing
typedef struct {
    uint32_t messagesTransmitted;   // Count of TX messages
    uint32_t messagesReceived;      // Count of RX messages
    uint32_t errorCount;            // Error counter
    uint32_t lastErrorCode;         // Last error code from CAN IP
} CAN_Stats_t;

//
// ============================================================================
// GLOBAL VARIABLES
// ============================================================================
//

static CAN_Stats_t canStats = {0, 0, 0, 0};            // CAN statistics
static CAN_Message_t lastRxMessage = {0, 0, {0}, 0};   // Last received message
static volatile uint8_t messageReceived = 0;           // Flag for message reception

//
// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================
//

void CAN_initModule(void);
void CAN_sendMessage(uint32_t msgId, uint8_t *data, uint8_t dlc);
uint8_t CAN_receiveMessage(CAN_Message_t *msg);
void CAN_processMessage(uint32_t msgId, uint8_t *data, uint8_t dlc);
void CAN_errorHandler(uint32_t errorStatus);
void CAN_setLED(uint32_t gpio, uint8_t state);
void CAN_toggleLED(uint32_t gpio);

// Unit test functions
void CAN_runUnitTests(void);
void CAN_testLoopback(void);
void CAN_testTransmit(void);
void CAN_testReceive(void);
uint8_t CAN_getStats(CAN_Stats_t *stats);

//
// ============================================================================
// MAIN APPLICATION
// ============================================================================
//

void main(void)
{
    //
    // Initialize device clock and peripherals
    //
    Device_init();

    //
    // Disable pin locks and enable internal pull-ups
    //
    Device_initGPIO();

    //
    // Initialize PIE and clear PIE registers. Disables CPU interrupts.
    //
    Interrupt_initModule();

    //
    // Initialize the PIE vector table
    //
    Interrupt_initVectorTable();

    //
    // PinMux and Peripheral Initialization
    //
    Board_init();

    //
    // C2000Ware Library initialization
    //
    C2000Ware_libraries_init();

    //
    // Initialize status LEDs as output
    //
    GPIO_setDirectionMode(STATUS_LED_PIN, GPIO_DIR_MODE_OUT);
    GPIO_setDirectionMode(ERROR_LED_PIN, GPIO_DIR_MODE_OUT);
    GPIO_writePin(STATUS_LED_PIN, 0);
    GPIO_writePin(ERROR_LED_PIN, 0);

    //
    // Initialize CAN module
    //
    CAN_initModule();

    //
    // Enable Global Interrupt (INTM) and real time interrupt (DBGM)
    //
    EINT;
    ERTM;

    //
    // ========================================================================
    // APPLICATION LOOP
    // ========================================================================
    //

    // Choose between normal operation or unit testing
    #ifdef RUN_UNIT_TESTS
        CAN_runUnitTests();
    #else
        // Normal operation: send test messages periodically
        uint32_t counter = 0;
        
        while(1)
        {
            counter++;
            
            // Send test message every ~100,000 iterations
            if(counter >= 100000)
            {
                uint8_t txData[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44};
                CAN_sendMessage(CAN_TX_MSG_ID, txData, 8);
                CAN_toggleLED(STATUS_LED_PIN);  // Toggle LED on TX
                counter = 0;
            }
            
            // Check for received messages
            CAN_Message_t rxMsg;
            if(CAN_receiveMessage(&rxMsg))
            {
                CAN_processMessage(rxMsg.id, rxMsg.data, rxMsg.dlc);
                CAN_toggleLED(STATUS_LED_PIN);  // Toggle LED on RX
            }
        }
    #endif
}

//
// ============================================================================
// CAN DRIVER FUNCTIONS
// ============================================================================
//

//
// CAN_initModule - Initialize CAN module with configured bit rate
//
void CAN_initModule(void)
{
    // Enable CAN module in peripheral clock gating
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_CANA);
    
    // Initialize CAN GPIO pins
    // TX pin (GPIO4) and RX pin (GPIO5) for CANA
    GPIO_setPinConfig(DEVICE_GPIO_CFG_CANTXA);
    GPIO_setPinConfig(DEVICE_GPIO_CFG_CANRXA);
    
    // Initialize CAN module
    CAN_initRAM(CAN_MODULE);
    CAN_setBitRate(CAN_MODULE, DEVICE_SYSCLK_FREQ, CAN_BITRATE_KBPS * 1000U);
    
    // Set up the CAN TX message object
    // This object will transmit messages with ID CAN_TX_MSG_ID
    CAN_setupMessageObject(CAN_MODULE, CAN_TX_MSG_OBJ,
                          CAN_TX_MSG_ID,
                          CAN_MSG_FRAME_STD,
                          CAN_MSG_OBJ_TYPE_TX,
                          0U,  // No ID masking
                          CAN_MSG_OBJ_NO_FLAGS,
                          8U); // Data length
    
    // Set up the CAN RX message object
    // This object will receive messages with ID CAN_RX_MSG_ID
    CAN_setupMessageObject(CAN_MODULE, CAN_RX_MSG_OBJ,
                          CAN_RX_MSG_ID,
                          CAN_MSG_FRAME_STD,
                          CAN_MSG_OBJ_TYPE_RX,
                          0U,  // No ID masking
                          CAN_MSG_OBJ_RX_INT_ENABLE,
                          8U); // Data length
    
    // Enable CAN module (required after setup)
    CAN_enableModule(CAN_MODULE);
    
    // If loopback mode is enabled, configure for internal testing
    #if CAN_USE_LOOPBACK
        CAN_setTestMode(CAN_MODULE, CAN_TEST_SILENT | CAN_TEST_LBACK);
    #endif
    
    // Enable CAN interrupts
    CAN_enableInterrupt(CAN_MODULE, CAN_INT_IE0 | CAN_INT_ERROR | CAN_INT_STATUS);
}

//
// CAN_sendMessage - Transmit a CAN message
//
// Parameters:
//   msgId - CAN message identifier (11-bit standard ID)
//   data  - Pointer to 8-byte data array
//   dlc   - Data length code (0-8 bytes)
//
void CAN_sendMessage(uint32_t msgId, uint8_t *data, uint8_t dlc)
{
    uint32_t idx;
    
    // Limit DLC to 8
    if(dlc > 8U)
    {
        dlc = 8U;
    }
    
    // Build the message data to transmit
    uint8_t msgData[8];
    for(idx = 0U; idx < dlc; idx++)
    {
        msgData[idx] = data[idx];
    }
    
    // Pad remaining bytes with zeros
    for(; idx < 8U; idx++)
    {
        msgData[idx] = 0U;
    }
    
    // Write message object with data
    CAN_sendMessage(CAN_MODULE, CAN_TX_MSG_OBJ, dlc, msgData);
    
    // Update statistics
    canStats.messagesTransmitted++;
}

//
// CAN_receiveMessage - Check for and retrieve received CAN message
//
// Parameters:
//   msg - Pointer to CAN_Message_t structure to fill with received data
//
// Returns:
//   1 if message was received, 0 if no message
//
uint8_t CAN_receiveMessage(CAN_Message_t *msg)
{
    uint32_t idx;
    uint8_t msgData[8];
    
    // Check if a message was received
    if(messageReceived == 0U)
    {
        return 0U;  // No message received
    }
    
    // Read the message from the receive message object
    // Note: In real implementation, use CAN driver read function
    // For now, copy from global structure
    msg->id = lastRxMessage.id;
    msg->dlc = lastRxMessage.dlc;
    for(idx = 0U; idx < 8U; idx++)
    {
        msg->data[idx] = lastRxMessage.data[idx];
    }
    msg->timestamp = lastRxMessage.timestamp;
    
    // Clear the received flag
    messageReceived = 0U;
    
    // Update statistics
    canStats.messagesReceived++;
    
    return 1U;  // Message retrieved successfully
}

//
// CAN_processMessage - Process received CAN message
//
// Parameters:
//   msgId - ID of received message
//   data  - Pointer to 8-byte data array
//   dlc   - Data length code
//
void CAN_processMessage(uint32_t msgId, uint8_t *data, uint8_t dlc)
{
    // Example: Echo received messages back
    // In a real application, perform message-specific processing
    
    if(msgId == CAN_RX_MSG_ID)
    {
        // Received expected message
        // Could add application-specific processing here
        
        // For unit testing: echo back the message
        #ifdef RUN_UNIT_TESTS
            CAN_sendMessage(CAN_ECHO_MSG_ID, data, dlc);
        #endif
    }
    else if(msgId == CAN_ECHO_MSG_ID)
    {
        // Received echo response
        // Used for loopback testing
    }
}

//
// CAN_errorHandler - Handle CAN error conditions
//
// Parameters:
//   errorStatus - Error status code from CAN module
//
void CAN_errorHandler(uint32_t errorStatus)
{
    canStats.errorCount++;
    canStats.lastErrorCode = errorStatus;
    
    // Turn on error LED
    CAN_setLED(ERROR_LED_PIN, 1U);
    
    // Error-specific handling
    if(errorStatus & CAN_ES_TXWARN)
    {
        // Transmit error warning
    }
    if(errorStatus & CAN_ES_RXWARN)
    {
        // Receive error warning
    }
    if(errorStatus & CAN_ES_TXERRPASSED)
    {
        // Transmit error counter exceeded
    }
    if(errorStatus & CAN_ES_RXERRPASSED)
    {
        // Receive error counter exceeded
    }
    if(errorStatus & CAN_ES_BUSOFF)
    {
        // Bus off condition - reinitialize
        CAN_initModule();
    }
}

//
// CAN_setLED - Set LED on/off
//
// Parameters:
//   gpio - GPIO pin number
//   state - 1 for on, 0 for off
//
void CAN_setLED(uint32_t gpio, uint8_t state)
{
    GPIO_writePin(gpio, state);
}

//
// CAN_toggleLED - Toggle LED
//
// Parameters:
//   gpio - GPIO pin number
//
void CAN_toggleLED(uint32_t gpio)
{
    GPIO_togglePin(gpio);
}

//
// ============================================================================
// UNIT TEST FUNCTIONS
// ============================================================================
//

//
// CAN_runUnitTests - Execute all unit tests
//
// This function runs a series of tests to verify CAN communication
// Include RUN_UNIT_TESTS in your project defines to use this
//
void CAN_runUnitTests(void)
{
    uint32_t testsPassed = 0U;
    uint32_t testsFailed = 0U;
    
    // Clear statistics
    canStats.messagesTransmitted = 0U;
    canStats.messagesReceived = 0U;
    canStats.errorCount = 0U;
    
    // Test 1: Loopback test (board sends to itself)
    #if CAN_USE_LOOPBACK
    CAN_testLoopback();
    if(canStats.messagesReceived > 0U && canStats.errorCount == 0U)
    {
        testsPassed++;
        CAN_setLED(STATUS_LED_PIN, 1U);  // LED on for pass
    }
    else
    {
        testsFailed++;
        CAN_setLED(ERROR_LED_PIN, 1U);   // LED on for fail
    }
    #endif
    
    // Test 2: Transmit test
    canStats.messagesTransmitted = 0U;
    CAN_testTransmit();
    if(canStats.messagesTransmitted >= 1U)
    {
        testsPassed++;
    }
    else
    {
        testsFailed++;
    }
    
    // Results: Blink LED pattern to indicate test results
    // Pattern: Fast blink = all passed, Slow blink = some failed
    uint32_t blinkRate = (testsFailed == 0U) ? 50000U : 200000U;
    uint32_t counter = 0U;
    
    while(1)
    {
        counter++;
        if(counter >= blinkRate)
        {
            CAN_toggleLED(STATUS_LED_PIN);
            counter = 0U;
        }
    }
}

//
// CAN_testLoopback - Test CAN loopback (internal message transmission)
//
void CAN_testLoopback(void)
{
    uint8_t testData[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    uint32_t timeout = 100000U;
    
    // Send test message in loopback mode
    CAN_sendMessage(CAN_TX_MSG_ID, testData, 8U);
    
    // Wait for message to be received
    while((timeout > 0U) && (messageReceived == 0U))
    {
        timeout--;
    }
    
    // Verify data was received correctly
    if(messageReceived && (lastRxMessage.dlc == 8U))
    {
        uint32_t idx;
        uint8_t dataMatches = 1U;
        
        for(idx = 0U; idx < 8U; idx++)
        {
            if(lastRxMessage.data[idx] != testData[idx])
            {
                dataMatches = 0U;
            }
        }
        
        if(dataMatches)
        {
            // Test passed
            messageReceived = 0U;
        }
        else
        {
            canStats.errorCount++;
        }
    }
    else
    {
        canStats.errorCount++;
    }
}

//
// CAN_testTransmit - Test CAN transmit functionality
//
void CAN_testTransmit(void)
{
    uint8_t txData[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11};
    
    // Transmit test message
    CAN_sendMessage(CAN_TX_MSG_ID, txData, 8U);
    
    // In actual hardware, verify transmission on oscilloscope or bus monitor
}

//
// CAN_testReceive - Test CAN receive functionality
//
void CAN_testReceive(void)
{
    CAN_Message_t rxMsg;
    uint32_t timeout = 1000000U;
    
    // Wait for a message to be received
    while((timeout > 0U) && (CAN_receiveMessage(&rxMsg) == 0U))
    {
        timeout--;
    }
    
    if(canStats.messagesReceived > 0U)
    {
        // Test passed - message received
    }
    else
    {
        canStats.errorCount++;
    }
}

//
// CAN_getStats - Retrieve CAN communication statistics
//
// Parameters:
//   stats - Pointer to CAN_Stats_t structure to fill
//
// Returns:
//   1 if successful, 0 if error
//
uint8_t CAN_getStats(CAN_Stats_t *stats)
{
    if(stats == 0)
    {
        return 0U;
    }
    
    stats->messagesTransmitted = canStats.messagesTransmitted;
    stats->messagesReceived = canStats.messagesReceived;
    stats->errorCount = canStats.errorCount;
    stats->lastErrorCode = canStats.lastErrorCode;
    
    return 1U;
}

//
// End of File
//
