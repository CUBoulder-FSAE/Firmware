#pragma once
#include "driverlib.h"

void inverter_transmit_init();

void inverter_transmit_set_current(uint8_t current);

void inverter_transmit_drive_enable();