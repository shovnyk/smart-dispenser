#pragma once

#define NUM_ELEMENTS_WINDOW 3
#define UPDATE_PERIOD_MS 500

void tmav_init();
bool tmav_insert(float element, float *average);
