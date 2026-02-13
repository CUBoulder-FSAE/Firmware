//
// CAN COMMUNICATION TEST SETUP GUIDE
// LaunchXL-F28P65X Dual Board Testing
//

/*
================================================================================
OVERVIEW
================================================================================

This guide explains how to set up and test CAN communication between two
LaunchXL-F28P65X boards using the provided CAN driver and unit test framework.

FILES INCLUDED:
- can_communication.c      : Main CAN driver with init, TX, RX functions
- can_communication.h      : Public API header
- can_unit_tests.c         : Comprehensive unit test suite
- CAN_TEST_README.txt      : This file

================================================================================
HARDWARE SETUP
================================================================================

PIN CONNECTIONS:
LaunchPad pinout for CAN module (CANA):
- GPIO 4  = CAN TX (CANTXA)
- GPIO 5  = CAN RX (CANRXA)
- GND     = Ground (common with other board)

PHYSICAL WIRING BETWEEN BOARDS:
1. Connect TX on Board A to RX on Board B
2. Connect RX on Board A to TX on Board B
3. Connect GND on both boards together
4. Use twisted pair or shielded cable for best signal integrity
5. Recommended CAN termination: 120Ω resistors at each end of bus

ALTERNATIVE SETUP (Star Configuration):
- Connect both board TX pins together (via 120Ω terminator)
- Connect both board RX pins together (via 120Ω terminator)
- Connect all GND together

================================================================================
CONFIGURATION
================================================================================

IMPORTANT: Read the configuration section in can_communication.c

Key parameters to modify:

1. CAN_BITRATE_KBPS (line ~50)
   - Default: 500 kbps
   - Options: 250, 500, 1000 kbps
   - Must be same on both boards
   - Change this if your application requires different speed

2. CAN_TX_MSG_ID (line ~53)
   - Default: 0x123
   - ID for transmit message object
   - Can be changed to any 11-bit value (0x000-0x7FF)

3. CAN_RX_MSG_ID (line ~54)
   - Default: 0x456
   - ID for receive message object
   - Board A should transmit to Board B's RX ID

4. CAN_USE_LOOPBACK (line ~56)
   - Set to 1: Board sends messages to itself (internal test)
   - Set to 0: Normal mode, receives from other board

5. STATUS_LED_PIN & ERROR_LED_PIN (lines ~59-60)
   - GPIO 12 = LED1 (status)
   - GPIO 13 = LED2 (error)
   - Modify if using different LED pins

================================================================================
COMPILATION & FLASHING
================================================================================

NORMAL OPERATION MODE (Dual Board Testing):

Board A (TX Board):
1. Open can_communication.c
2. Ensure CAN_USE_LOOPBACK = 0
3. Make CAN_TX_MSG_ID = 0x123
4. Make CAN_RX_MSG_ID = 0x456
5. Compile and flash

Board B (RX Board):
1. Open can_communication.c
2. Ensure CAN_USE_LOOPBACK = 0
3. Make CAN_TX_MSG_ID = 0x456 (swap TX/RX)
4. Make CAN_RX_MSG_ID = 0x123
5. Compile and flash

UNIT TEST MODE (Single Board Loopback):

Both Boards:
1. Open can_communication.c
2. Set CAN_USE_LOOPBACK = 1
3. Add -DRUN_UNIT_TESTS to compiler flags:
   - In CCS: Project → Properties → Build → Compiler → Symbols
   - Add "RUN_UNIT_TESTS" to predefined symbols
4. Compile and flash
5. Press reset button on LaunchPad

================================================================================
TESTING PROCEDURES
================================================================================

TEST 1: LOOPBACK TEST (Single Board)
--------
Purpose: Verify CAN module works without external connections
Steps:
1. Set CAN_USE_LOOPBACK = 1 in can_communication.c
2. Enable RUN_UNIT_TESTS compiler flag
3. Flash to a single board
4. Expected LED pattern:
   - LED1 steady ON = All tests passed
   - LED2 steady ON = Tests failed
   - Fast blink = Multiple passes
   - Slow blink = Some failures

TEST 2: DUAL BOARD COMMUNICATION
--------
Purpose: Verify data transmission between two boards

Board A (TX):
1. Set CAN_USE_LOOPBACK = 0
2. Ensure CAN_TX_MSG_ID = 0x123
3. Ensure CAN_RX_MSG_ID = 0x456
4. Build and flash

Board B (RX):
1. Set CAN_USE_LOOPBACK = 0
2. Ensure CAN_TX_MSG_ID = 0x456
3. Ensure CAN_RX_MSG_ID = 0x123
4. Build and flash

Expected Behavior:
- Board A continuously transmits test messages every ~100ms
- Board B receives messages and echoes back
- Both LEDs toggle with activity (indicates RX/TX)
- If communication fails:
  - Check physical connections
  - Verify CAN termination (120Ω resistors)
  - Check bit rate configuration matches
  - Check message IDs are swapped correctly

TEST 3: UNIT TEST SUITE (Comprehensive)
--------
Purpose: Run all test cases to verify functionality

Steps:
1. Flash firmware with RUN_UNIT_TESTS enabled
2. Monitor LED pattern:
   - LED1 flashes = Passed tests
   - LED2 flashes = Failed tests
3. Count LED flashes to verify all 6 tests completed
4. Check CCS debug output (if serial monitor available)

Test Coverage:
✓ Test 1: CAN module initialization
✓ Test 2: Single message transmission
✓ Test 3: Message reception
✓ Test 4: Data integrity verification
✓ Test 5: Sequential message transmission
✓ Test 6: Error handling

================================================================================
MODIFYING MESSAGE CONTENT
================================================================================

To send custom data instead of test pattern:

1. In main loop of can_communication.c (~line 140), modify:

   uint8_t txData[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44};
   
   Replace 0xAA-0x44 with your custom data bytes

2. To send fewer than 8 bytes, modify DLC:

   CAN_sendMessage(CAN_TX_MSG_ID, txData, 4);  // Send only 4 bytes
   
3. To process received messages, modify CAN_processMessage():

   void CAN_processMessage(uint32_t msgId, uint8_t *data, uint8_t dlc)
   {
       // Add your message handling here
       if(msgId == CAN_RX_MSG_ID)
       {
           // Process received data
           // data[0], data[1], ... data[7]
       }
   }

================================================================================
TROUBLESHOOTING
================================================================================

PROBLEM: No communication between boards
SOLUTION:
- Verify physical CAN connections (TX to RX, RX to TX)
- Check GND connection between boards
- Verify both boards have same bit rate (500 kbps default)
- Add 120Ω termination resistors between TX and RX on each board
- Check that message IDs are swapped between boards (A TX = B RX, etc.)

PROBLEM: One board receives, other doesn't
SOLUTION:
- Verify message ID configuration matches
- Check Board A TX_MSG_ID = Board B RX_MSG_ID
- Check Board B TX_MSG_ID = Board A RX_MSG_ID
- LED should toggle when messages are sent/received

PROBLEM: CAN bus error LED (LED2) is on
SOLUTION:
- Check for short circuits in wiring
- Verify proper termination (120Ω resistor)
- Reduce bit rate (try 250 kbps instead of 500 kbps)
- Check for electromagnetic interference

PROBLEM: Tests pass in loopback but fail in dual board mode
SOLUTION:
- Physical layer issue, check wiring again
- Try shorter CAN cable
- Verify both boards are powered
- Check bit rate matches exactly

================================================================================
ADVANCING TO YOUR APPLICATION
================================================================================

To use this CAN driver in your project:

1. Keep can_communication.c and can_communication.h
2. Remove can_unit_tests.c (or keep it as reference)
3. Include can_communication.h in your main file:
   #include "can_communication.h"

4. Initialize CAN in main():
   CAN_initModule();

5. Send messages:
   uint8_t data[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
   CAN_sendMessage(0x123, data, 8);

6. Receive messages:
   CAN_Message_t rxMsg;
   if(CAN_receiveMessage(&rxMsg))
   {
       // Process rxMsg
   }

7. Handle errors by modifying CAN_errorHandler():
   void CAN_errorHandler(uint32_t errorStatus)
   {
       // Add your error handling code
   }

8. Process received messages by modifying CAN_processMessage():
   void CAN_processMessage(uint32_t msgId, uint8_t *data, uint8_t dlc)
   {
       // Add your message processing code
   }

================================================================================
PERFORMANCE CONSIDERATIONS
================================================================================

Default Configuration (500 kbps, 8-byte messages):
- Minimum time between messages: ~125 µs
- Maximum message rate: ~1000 messages/second (per direction)
- Current implementation: ~100 ms between test messages (10 msg/sec)
- Practical limit: Limited by message processing overhead

To increase message rate:
- Reduce delay in main loop (change 100000 to smaller value)
- Use interrupt-driven reception instead of polling
- Optimize message processing code

To decrease CAN bus load:
- Increase delay between messages
- Use fewer data bytes (DLC < 8)
- Reduce bit rate (250 kbps uses less bandwidth)

================================================================================
END OF DOCUMENTATION
================================================================================
*/
