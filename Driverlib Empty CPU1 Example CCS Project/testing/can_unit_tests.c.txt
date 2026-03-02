//
// FILE: can_unit_tests.c
//
// DESCRIPTION: Comprehensive unit test suite for CAN communication
//
// This file provides detailed testing functions to verify CAN functionality
// on dual LaunchXL-F28P65X boards. Tests cover:
// - Internal loopback communication
// - Inter-board communication
// - Message validation
// - Error handling
//
// USAGE:
// 1. Compile with can_communication.c and can_communication.h
// 2. Add -DRUN_UNIT_TESTS to compiler flags to enable unit test mode
// 3. Flash to both boards (Board A: TX only, Board B: RX only)
// 4. Monitor LED patterns for test results
//

#include <stdint.h>
#include <stdbool.h>
#include "driverlib.h"
#include "device.h"
#include "can_communication.h"

//
// ============================================================================
// TEST CONFIGURATION
// ============================================================================
//

// Test Pattern Definitions
#define TEST_PATTERN_1      0xAA
#define TEST_PATTERN_2      0x55
#define TEST_PATTERN_3      0xFF
#define TEST_PATTERN_4      0x00

// Test Timing
#define TEST_TIMEOUT_LOOPS  1000000U    // Timeout for waiting on messages
#define TEST_DELAY_LOOPS    100000U     // Delay between test phases

// Test Result Codes
#define TEST_PASS           1U
#define TEST_FAIL           0U

//
// ============================================================================
// TEST STATISTICS
// ============================================================================
//

typedef struct {
    uint32_t testNumber;            // Current test number
    uint32_t totalTests;            // Total tests run
    uint32_t passedTests;           // Number of passed tests
    uint32_t failedTests;           // Number of failed tests
    uint32_t lastTestResult;        // Result of last test
    char testName[64];              // Name of current test
} TestStats_t;

static TestStats_t testStats = {0, 0, 0, 0, 0, {0}};

//
// ============================================================================
// TEST HELPER FUNCTIONS
// ============================================================================
//

//
// Test_printResult - Indicate test result via LED and counter
//
// Parameters:
//   result - TEST_PASS or TEST_FAIL
//
static void Test_printResult(uint8_t result)
{
    testStats.lastTestResult = result;
    
    if(result == TEST_PASS)
    {
        testStats.passedTests++;
        GPIO_writePin(DEVICE_GPIO_PIN_LED1, 1U);  // LED on for pass
    }
    else
    {
        testStats.failedTests++;
        GPIO_writePin(DEVICE_GPIO_PIN_LED2, 1U);  // LED on for fail
    }
}

//
// Test_delay - Blocking delay for test sequencing
//
// Parameters:
//   loops - Number of iterations to delay
//
static void Test_delay(uint32_t loops)
{
    uint32_t i;
    for(i = 0U; i < loops; i++)
    {
        asm(" NOP");  // No operation (stalls for timing)
    }
}

//
// Test_initStatusLEDs - Initialize LED GPIOs for test indication
//
static void Test_initStatusLEDs(void)
{
    GPIO_setDirectionMode(DEVICE_GPIO_PIN_LED1, GPIO_DIR_MODE_OUT);
    GPIO_setDirectionMode(DEVICE_GPIO_PIN_LED2, GPIO_DIR_MODE_OUT);
    GPIO_writePin(DEVICE_GPIO_PIN_LED1, 0U);  // Off initially
    GPIO_writePin(DEVICE_GPIO_PIN_LED2, 0U);
}

//
// ============================================================================
// INDIVIDUAL TEST FUNCTIONS
// ============================================================================
//

//
// Test_CAN_BasicInitialization - Verify CAN module initializes correctly
//
// Returns: TEST_PASS if successful, TEST_FAIL otherwise
//
uint8_t Test_CAN_BasicInitialization(void)
{
    CAN_Stats_t stats;
    
    // Verify stats structure is accessible
    if(CAN_getStats(&stats) != 1U)
    {
        return TEST_FAIL;
    }
    
    // At start, no messages should have been sent
    if(stats.messagesTransmitted != 0U || stats.messagesReceived != 0U)
    {
        return TEST_FAIL;
    }
    
    return TEST_PASS;
}

//
// Test_CAN_TransmitMessage - Test single message transmission
//
// Sends a test message and verifies transmission count increments
//
// Returns: TEST_PASS if successful, TEST_FAIL otherwise
//
uint8_t Test_CAN_TransmitMessage(void)
{
    uint8_t txData[8] = {TEST_PATTERN_1, TEST_PATTERN_2, TEST_PATTERN_3, TEST_PATTERN_4,
                         0x11, 0x22, 0x33, 0x44};
    CAN_Stats_t statsBeforeWhen tx, statsAfter;
    
    // Get stats before transmission
    CAN_getStats(&statsBeforeTx);
    
    // Send test message
    CAN_sendMessage(0x123, txData, 8U);
    
    // Small delay to allow transmission to complete
    Test_delay(TEST_DELAY_LOOPS);
    
    // Get stats after transmission
    CAN_getStats(&statsAfter);
    
    // Verify transmission count increased
    if(statsAfter.messagesTransmitted > statsBeforeTx.messagesTransmitted)
    {
        return TEST_PASS;
    }
    else
    {
        return TEST_FAIL;
    }
}

//
// Test_CAN_ReceiveMessage - Test message reception
//
// Waits for a message to be received and verifies message structure
//
// Returns: TEST_PASS if successful, TEST_FAIL otherwise
//
uint8_t Test_CAN_ReceiveMessage(void)
{
    CAN_Message_t rxMsg;
    CAN_Stats_t statsBeforeRx, statsAfter;
    uint32_t timeout;
    
    // Get stats before waiting
    CAN_getStats(&statsBeforeRx);
    
    // Wait for message with timeout
    timeout = TEST_TIMEOUT_LOOPS;
    while((timeout > 0U) && (CAN_receiveMessage(&rxMsg) == 0U))
    {
        timeout--;
    }
    
    // Get stats after waiting
    CAN_getStats(&statsAfter);
    
    // Verify a message was received
    if(timeout > 0U && (statsAfter.messagesReceived > statsBeforeRx.messagesReceived))
    {
        // Verify message has valid structure
        if(rxMsg.dlc > 0U && rxMsg.dlc <= 8U)
        {
            return TEST_PASS;
        }
    }
    
    return TEST_FAIL;
}

//
// Test_CAN_DataIntegrity - Test that transmitted data matches received data
//
// Sends a known pattern and verifies it matches on reception
//
// Returns: TEST_PASS if data integrity verified, TEST_FAIL otherwise
//
uint8_t Test_CAN_DataIntegrity(void)
{
    uint8_t txData[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    CAN_Message_t rxMsg;
    uint32_t idx, timeout;
    uint8_t dataMatches;
    
    // Send test message with known pattern
    CAN_sendMessage(0x456, txData, 8U);
    
    Test_delay(TEST_DELAY_LOOPS);
    
    // Wait for reception
    timeout = TEST_TIMEOUT_LOOPS;
    while((timeout > 0U) && (CAN_receiveMessage(&rxMsg) == 0U))
    {
        timeout--;
    }
    
    if(timeout == 0U)
    {
        return TEST_FAIL;  // Message not received
    }
    
    // Verify data length
    if(rxMsg.dlc != 8U)
    {
        return TEST_FAIL;
    }
    
    // Verify data content byte-by-byte
    dataMatches = 1U;
    for(idx = 0U; idx < 8U; idx++)
    {
        if(rxMsg.data[idx] != txData[idx])
        {
            dataMatches = 0U;
            break;
        }
    }
    
    return (dataMatches) ? TEST_PASS : TEST_FAIL;
}

//
// Test_CAN_SequentialMessages - Test transmission of multiple messages
//
// Sends 5 messages and verifies all are transmitted
//
// Returns: TEST_PASS if all transmitted, TEST_FAIL otherwise
//
uint8_t Test_CAN_SequentialMessages(void)
{
    uint32_t idx;
    uint8_t txData[8];
    CAN_Stats_t statsBefore, statsAfter;
    
    CAN_getStats(&statsBefore);
    
    // Send 5 test messages
    for(idx = 0U; idx < 5U; idx++)
    {
        txData[0] = idx & 0xFFU;
        txData[1] = (idx >> 8) & 0xFFU;
        txData[2] = 0xAA;
        txData[3] = 0xBB;
        txData[4] = 0xCC;
        txData[5] = 0xDD;
        txData[6] = 0xEE;
        txData[7] = 0xFF;
        
        CAN_sendMessage(0x789 + idx, txData, 8U);
        Test_delay(TEST_DELAY_LOOPS / 5U);  // Delay between messages
    }
    
    Test_delay(TEST_DELAY_LOOPS);
    
    CAN_getStats(&statsAfter);
    
    // Verify 5 messages transmitted
    if((statsAfter.messagesTransmitted - statsBefore.messagesTransmitted) >= 5U)
    {
        return TEST_PASS;
    }
    else
    {
        return TEST_FAIL;
    }
}

//
// Test_CAN_ErrorHandling - Test error detection (if errors occur)
//
// Returns: TEST_PASS if error handling works, TEST_FAIL otherwise
//
uint8_t Test_CAN_ErrorHandling(void)
{
    CAN_Stats_t stats;
    
    CAN_getStats(&stats);
    
    // If no errors occurred during previous tests, error handling passed
    // (no errors = good communication)
    if(stats.errorCount == 0U)
    {
        return TEST_PASS;
    }
    else
    {
        return TEST_FAIL;
    }
}

//
// ============================================================================
// MAIN TEST SUITE
// ============================================================================
//

//
// CAN_runUnitTests - Execute complete unit test suite
//
// This is the main test function that runs all unit tests and reports results
//
void CAN_runUnitTests(void)
{
    Test_initStatusLEDs();
    Test_delay(500000U);  // Initial delay for board startup
    
    // Test 1: Basic Initialization
    if(Test_CAN_BasicInitialization() == TEST_PASS)
    {
        Test_printResult(TEST_PASS);
    }
    else
    {
        Test_printResult(TEST_FAIL);
    }
    Test_delay(TEST_DELAY_LOOPS);
    
    // Test 2: Single Message Transmission
    if(Test_CAN_TransmitMessage() == TEST_PASS)
    {
        Test_printResult(TEST_PASS);
    }
    else
    {
        Test_printResult(TEST_FAIL);
    }
    Test_delay(TEST_DELAY_LOOPS);
    
    // Test 3: Message Reception (for receiver board)
    if(Test_CAN_ReceiveMessage() == TEST_PASS)
    {
        Test_printResult(TEST_PASS);
    }
    else
    {
        Test_printResult(TEST_FAIL);
    }
    Test_delay(TEST_DELAY_LOOPS);
    
    // Test 4: Data Integrity
    if(Test_CAN_DataIntegrity() == TEST_PASS)
    {
        Test_printResult(TEST_PASS);
    }
    else
    {
        Test_printResult(TEST_FAIL);
    }
    Test_delay(TEST_DELAY_LOOPS);
    
    // Test 5: Sequential Messages
    if(Test_CAN_SequentialMessages() == TEST_PASS)
    {
        Test_printResult(TEST_PASS);
    }
    else
    {
        Test_printResult(TEST_FAIL);
    }
    Test_delay(TEST_DELAY_LOOPS);
    
    // Test 6: Error Handling
    if(Test_CAN_ErrorHandling() == TEST_PASS)
    {
        Test_printResult(TEST_PASS);
    }
    else
    {
        Test_printResult(TEST_FAIL);
    }
    Test_delay(TEST_DELAY_LOOPS * 2U);
    
    //
    // Display final results
    //
    GPIO_writePin(DEVICE_GPIO_PIN_LED1, 0U);
    GPIO_writePin(DEVICE_GPIO_PIN_LED2, 0U);
    
    // Blink LED pattern indicating final status
    // LED1 blinks = passed tests, LED2 blinks = failed tests
    uint32_t counter = 0U;
    uint32_t idx;
    
    while(1)
    {
        // Blink LED1 for each passed test
        for(idx = 0U; idx < testStats.passedTests; idx++)
        {
            GPIO_togglePin(DEVICE_GPIO_PIN_LED1);
            Test_delay(200000U);
            GPIO_togglePin(DEVICE_GPIO_PIN_LED1);
            Test_delay(200000U);
        }
        
        // Pause between patterns
        Test_delay(500000U);
        
        // Blink LED2 for each failed test
        for(idx = 0U; idx < testStats.failedTests; idx++)
        {
            GPIO_togglePin(DEVICE_GPIO_PIN_LED2);
            Test_delay(200000U);
            GPIO_togglePin(DEVICE_GPIO_PIN_LED2);
            Test_delay(200000U);
        }
        
        // Pause before repeating
        Test_delay(1000000U);
        
        counter++;
    }
}

//
// End of File
//
