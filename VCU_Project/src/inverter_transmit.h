#pragma once
#include "driverlib.h"

/**
 * Initilaize CAN Message Objects
 */
void inverter_transmit_init();

/**
 * Set requested current value from the inverter
 * @param current 0-200
 */
void inverter_transmit_set_current(uint8_t current);

/**
 * Set drive enable to 1
 */
void inverter_transmit_drive_enable();