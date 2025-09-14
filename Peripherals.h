#pragma once

#include "Arduino.h"

void peripherals_init();
int system_check(int attempt_number);
void dispense(bool start, int *quantity);
