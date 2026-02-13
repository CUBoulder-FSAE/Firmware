//
// FILE: can_communication.h
//
// DESCRIPTION: CAN communication driver header
//              Public API for CAN transmit, receive, and unit testing
//

#ifndef CAN_COMMUNICATION_H
#define CAN_COMMUNICATION_H

#include <stdint.h>
#include <stdbool.h>

//
// CAN Message Structure
//
typedef struct {
    uint32_t id;                // Message ID (11-bit standard)
    uint8_t dlc;                // Data Length Code (0-8)
    uint8_t data[8];            // Message data bytes
    uint32_t timestamp;         // Timestamp of message
} CAN_Message_t;

//
// CAN Statistics Structure for Unit Testing
//
typedef struct {
    uint32_t messagesTransmitted;   // Count of successfully transmitted messages
    uint32_t messagesReceived;      // Count of successfully received messages
    uint32_t errorCount;            // Total error count
    uint32_t lastErrorCode;         // Most recent error code
} CAN_Stats_t;

//
// ============================================================================
// PUBLIC API FUNCTIONS
// ============================================================================
//

//
// CAN_initModule - Initialize CAN module with configured bit rate
//
// This function must be called once during initialization.
// Configures GPIO pins, sets up message objects, and enables interrupts.
//
void CAN_initModule(void);

//
// CAN_sendMessage - Transmit a CAN message
//
// Parameters:
//   msgId - CAN message identifier (11-bit standard)
//   data  - Pointer to 8-byte data array
//   dlc   - Data length code (0-8 bytes)
//
// Example:
//   uint8_t myData[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
//   CAN_sendMessage(0x123, myData, 8);
//
void CAN_sendMessage(uint32_t msgId, uint8_t *data, uint8_t dlc);

//
// CAN_receiveMessage - Check for and retrieve received CAN message
//
// Parameters:
//   msg - Pointer to CAN_Message_t structure to receive data
//
// Returns:
//   1 if message was received and copied, 0 if no new message
//
// Example:
//   CAN_Message_t rxMsg;
//   if(CAN_receiveMessage(&rxMsg))
//   {
//       // Process rxMsg.id, rxMsg.data, rxMsg.dlc
//   }
//
uint8_t CAN_receiveMessage(CAN_Message_t *msg);

//
// CAN_processMessage - Process received CAN message (application callback)
//
// Parameters:
//   msgId - ID of received message
//   data  - Pointer to 8-byte data array
//   dlc   - Data length code
//
// This function is called internally when a message is received.
// Override in your application code to handle message-specific logic.
//
void CAN_processMessage(uint32_t msgId, uint8_t *data, uint8_t dlc);

//
// CAN_errorHandler - Handle CAN error conditions (application callback)
//
// Parameters:
//   errorStatus - Error status code from CAN module
//
// This function is called when CAN errors occur.
// Override in your application code to handle errors appropriately.
//
void CAN_errorHandler(uint32_t errorStatus);

//
// CAN_setLED - Set LED output (helper function)
//
// Parameters:
//   gpio - GPIO pin number
//   state - 1 for on, 0 for off
//
void CAN_setLED(uint32_t gpio, uint8_t state);

//
// CAN_toggleLED - Toggle LED output (helper function)
//
// Parameters:
//   gpio - GPIO pin number
//
void CAN_toggleLED(uint32_t gpio);

//
// ============================================================================
// UNIT TEST API - Include RUN_UNIT_TESTS in project defines to enable
// ============================================================================
//

//
// CAN_runUnitTests - Execute all CAN unit tests
//
// This function runs a suite of tests including:
//   - Loopback test (if CAN_USE_LOOPBACK enabled)
//   - Transmit test
//   - Receive test
//
// Results are indicated by LED patterns:
//   - Fast blink: All tests passed
//   - Slow blink: Some tests failed
//
// This function does not return (infinite loop with status indication)
//
void CAN_runUnitTests(void);

//
// CAN_testLoopback - Test CAN internal loopback (sends and receives own message)
//
// Prerequisites: CAN_USE_LOOPBACK must be set to 1
//
void CAN_testLoopback(void);

//
// CAN_testTransmit - Test CAN transmit functionality
//
// Sends a test message on the CAN bus.
// Use with a CAN bus monitor or oscilloscope to verify transmission.
//
void CAN_testTransmit(void);

//
// CAN_testReceive - Test CAN receive functionality
//
// Waits for a message to be received on the CAN bus.
// Call this on a board configured to receive from another board.
//
void CAN_testReceive(void);

//
// CAN_getStats - Retrieve CAN communication statistics
//
// Parameters:
//   stats - Pointer to CAN_Stats_t structure to receive data
//
// Returns:
//   1 if successful, 0 if error
//
// Example:
//   CAN_Stats_t myStats;
//   CAN_getStats(&myStats);
//   // Access: myStats.messagesTransmitted, myStats.messagesReceived, etc.
//
uint8_t CAN_getStats(CAN_Stats_t *stats);

#endif  // CAN_COMMUNICATION_H

//
// End of File
//
