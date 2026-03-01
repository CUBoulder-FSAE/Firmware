//#############################################################################
//
// FILE:   empty_driverlib_main.c
//
//! \addtogroup driver_example_list
//! <h1>Empty Project Example</h1> 
//!
//! This example is an empty project setup for Driverlib development.
//!
//
//#############################################################################
//
// test
// 
// C2000Ware v6.00.01.00
//
// Copyright (C) 2024 Texas Instruments Incorporated - http://www.ti.com
//
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions 
// are met:
// 
//   Redistributions of source code must retain the above copyright 
//   notice, this list of conditions and the following disclaimer.
// 
//   Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the 
//   documentation and/or other materials provided with the   
//   distribution.
// 
//   Neither the name of Texas Instruments Incorporated nor the names of
//   its contributors may be used to endorse or promote products derived
//   from this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// $
//#############################################################################

//
// Included Files
//
#include "driverlib.h"
#include "device.h"
#include "board.h"
#include "c2000ware_libraries.h"

//
// ============================================================================
// CONFIGURATION PARAMETERS - Modify these for your specific application
// ============================================================================
//

// ADC Configuration
#define ADC_INSTANCE            ADCA_BASE           // ADC module to use (ADCA, ADCB, ADCC, ADCD)
#define ADC_CHANNEL             ADC_CH_ADCIN0       // Input channel (ADC_CH_ADCIN0 to ADCIN15)
#define ADC_SOC_NUMBER          ADC_SOC_NUMBER0     // Start of Conversion number (0-15)

// LED Configuration
#define LED_GPIO_PIN            DEVICE_GPIO_PIN_LED1     // GPIO pin for LED (12 = LED1, 13 = LED2)
#define LED_GPIO_CFG            DEVICE_GPIO_CFG_LED1     // GPIO configuration for LED

// ADC to Frequency Mapping
// These values map ADC readings to LED blink frequencies
// ADC is 12-bit (0-4095), map to frequency range
#define MIN_ADC_VALUE           0       // Minimum ADC reading
#define MAX_ADC_VALUE           4095    // Maximum ADC reading (12-bit)
#define MIN_BLINK_FREQ_HZ       1       // Minimum blink frequency in Hz (slower)
#define MAX_BLINK_FREQ_HZ       10      // Maximum blink frequency in Hz (faster)

// CPU Timer Configuration for LED blinking
#define LED_TIMER_BASE          CPUTIMER0_BASE      // Timer for LED blinking control
#define LED_TIMER_FREQ_HZ       (DEVICE_SYSCLK_FREQ / 100)  // Timer update frequency

//
// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================
//

void initADC(void);
void initLED(void);
void initTimer(void);
void configureADCTrigger(void);
uint32_t readADCValue(void);
void controlLEDFrequency(uint32_t adcValue);

//
// ============================================================================
// GLOBAL VARIABLES
// ============================================================================
//

// LED blinking state machine variables
static uint16_t ledBlinkCounter = 0;      // Counter for LED toggle timing
static uint16_t ledBlinkPeriod = 50;      // Period between LED toggles (in 10ms intervals)
static uint8_t ledState = 0;              // Current LED state (0 = off, 1 = on)

//
// Main
//
void main(void)
{
    //
    // Initialize device clock and peripherals
    //
    Device_init();

    //
    // Disable pin locks and enable internal pull-ups.
    //
    Device_initGPIO();

    //
    // Initialize PIE and clear PIE registers. Disables CPU interrupts.
    //
    Interrupt_initModule();

    //
    // Initialize the PIE vector table with pointers to the shell Interrupt
    // Service Routines (ISR).
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
    // ========================================================================
    // APPLICATION-SPECIFIC INITIALIZATION
    // ========================================================================
    //

    // Initialize LED GPIO pin for output
    initLED();

    // Initialize ADC for voltage measurement
    initADC();

    // Initialize CPU Timer for blinking control
    initTimer();

    //
    // Enable Global Interrupt (INTM) and real time interrupt (DBGM)
    //
    EINT;
    ERTM;

    //
    // ========================================================================
    // MAIN APPLICATION LOOP
    // ========================================================================
    //
    while(1)
    {
        // Read ADC value from selected channel
        uint32_t adcValue = readADCValue();
        
        // Control LED blinking frequency based on ADC reading
        // Higher ADC voltage = faster blinking
        controlLEDFrequency(adcValue);
    }
}

//
// ============================================================================
// FUNCTION IMPLEMENTATIONS
// ============================================================================
//

//
// initLED - Initialize LED GPIO pin for output
//
void initLED(void)
{
    // Configure LED pin as GPIO output
    GPIO_setDirectionMode(LED_GPIO_PIN, GPIO_DIR_MODE_OUT);
    
    // Initialize LED to OFF state
    GPIO_writePin(LED_GPIO_PIN, 0);
}

//
// initADC - Initialize ADC module for voltage measurement
//
void initADC(void)
{
    // Power up ADC
    ADC_enableConverter(ADC_INSTANCE);
    
    // Delay for ADC to stabilize (required after power up)
    DEVICE_DELAY_US(1000);
    
    // Configure ADC clock divider
    // Set clock to reasonable speed (adjust clock prescaler as needed)
    ADC_setPrescaler(ADC_INSTANCE, ADC_CLK_DIV_2_0);
    
    // Set resolution to 12-bit
    ADC_setResolution(ADC_INSTANCE, ADC_RESOLUTION_12BIT);
    
    // Set ADC signal mode to single-ended
    ADC_setSignalMode(ADC_INSTANCE, ADC_MODE_SINGLE_ENDED);
    
    // Configure ADC SOC (Start of Conversion)
    // Set the ADC channel and acquisition window
    ADC_setupSOC(ADC_INSTANCE, 
                 ADC_SOC_NUMBER,        // SOC number to configure
                 ADC_TRIGGER_SW_ONLY,   // Software-triggered (manual)
                 ADC_CHANNEL,           // Select input channel
                 10U);                  // Acquisition window (sample time in ADCCLK cycles)
    
    // Enable continuous conversion mode
    ADC_setInterruptSource(ADC_INSTANCE, ADC_INT_NUMBER1, ADC_SOC_NUMBER);
    ADC_enableInterrupt(ADC_INSTANCE, ADC_INT_NUMBER1);
}

//
// initTimer - Initialize CPU Timer for periodic blinking update
//
void initTimer(void)
{
    // Configure Timer frequency (e.g., 100 Hz = 10ms intervals)
    uint32_t timerPeriod = DEVICE_SYSCLK_FREQ / 100;  // For 100Hz update rate
    
    // Stop timer during configuration
    CPUTimer_stopTimer(LED_TIMER_BASE);
    
    // Configure timer period
    CPUTimer_setPeriod(LED_TIMER_BASE, timerPeriod);
    
    // Set timer in continuous mode
    CPUTimer_setMode(LED_TIMER_BASE, CPUTIMER_MODE_CONTINUOUS);
    
    // Start the timer
    CPUTimer_startTimer(LED_TIMER_BASE);
}

//
// readADCValue - Read ADC value from configured channel
//
// Returns: 12-bit ADC value (0-4095)
//
uint32_t readADCValue(void)
{
    uint32_t adcValue;
    
    // Force start of conversion on selected SOC
    ADC_forceSOC(ADC_INSTANCE, ADC_SOC_NUMBER);
    
    // Small delay for conversion to complete (adjust if needed)
    // Typical conversion time is ~10-20 ADCCLK cycles
    DEVICE_DELAY_US(100);
    
    // Read result from the result register
    adcValue = ADC_readResult(ADC_INSTANCE, ADC_RES_FIFO1);
    
    return adcValue;
}

//
// controlLEDFrequency - Control LED blinking frequency based on ADC voltage
//
// Maps ADC reading to blink frequency:
//   - Low ADC voltage  = slow blinking
//   - High ADC voltage = fast blinking
//
// Input: adcValue - 12-bit ADC reading (0-4095)
//
void controlLEDFrequency(uint32_t adcValue)
{
    // Map ADC value to blink frequency
    // Formula: frequency = MIN_FREQ + (adcValue / MAX_ADC) * (MAX_FREQ - MIN_FREQ)
    
    // Scale ADC to frequency range
    uint32_t freqHz = MIN_BLINK_FREQ_HZ + 
                      ((adcValue * (MAX_BLINK_FREQ_HZ - MIN_BLINK_FREQ_HZ)) / MAX_ADC_VALUE);
    
    // Convert frequency to period (in 10ms intervals)
    // Period = (1 / frequency) / 0.01
    // Divide by 2 because we toggle twice per period (on->off, off->on)
    ledBlinkPeriod = (100 / (2 * freqHz));
    
    // Ensure minimum period to avoid division issues
    if(ledBlinkPeriod < 1)
    {
        ledBlinkPeriod = 1;
    }
    
    // Increment the counter
    ledBlinkCounter++;
    
    // Toggle LED when counter reaches the blink period
    if(ledBlinkCounter >= ledBlinkPeriod)
    {
        // Toggle LED state
        ledState = !ledState;
        GPIO_writePin(LED_GPIO_PIN, ledState);
        
        // Reset counter
        ledBlinkCounter = 0;
    }
}

//
// End of File
//
