#include "TimedMovingAverageVector.h"
#include "Arduino.h"

static float window[NUM_ELEMENTS_WINDOW];
static long last_millis;
static int idx;
float moving_average;

static float average(float *arr, int len)
{
  float sum = 0;
  for (int i = 0; i < len; ++i)
    sum += arr[i];
  return sum/len;
}

void tmav_init()
{
  last_millis = millis();
  idx = 0;
  memset(window, 0, sizeof(window));
}

bool tmav_insert(float element, float *avg)
{
  if (element < 0) {
    return false;
  }
  // Note: another check to clean up float values that are outside of the 
  // expected range (outside of few SDs around the moving average).
  window[idx++ % NUM_ELEMENTS_WINDOW] = element;
  if (millis() - last_millis > UPDATE_PERIOD_MS) {
    last_millis = millis();
    moving_average = average(window, NUM_ELEMENTS_WINDOW);
    *avg = moving_average;
    return true;
  }
  return false;
}
