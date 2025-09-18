#pragma once

#include "Arduino.h"

void peripherals_init();
int system_check(int attempt_number);
void dispense(bool start, int *quantity);
void flow_sensor_calibrate(float calconst);
