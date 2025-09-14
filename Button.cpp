#include "Arduino.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "Button.h"

static int buttonA = 35;
static int buttonB = 34;

static volatile enum button_type btn = BUTTON_NONE;
static SemaphoreHandle_t button_semaphore;
static bool window_open = false;
static long long last_millis;

#define BUTTON_TIMEOUT              pdMS_TO_TICKS(3600000) // 1 hour.
#define BUTTON_DEBOUNCE_INTERVAL_MS 150

static void IRAM_ATTR buttonAPressed() {
  // Basic button debouncing.
  if (millis() - last_millis < BUTTON_DEBOUNCE_INTERVAL_MS) {
    return;
  }
  last_millis = millis();
  btn = BUTTON_A;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (window_open) {
    xSemaphoreGiveFromISR(button_semaphore, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken == pdTRUE) {
      portYIELD_FROM_ISR();
    }
  }
}

static void IRAM_ATTR buttonBPressed() {
  if (millis() - last_millis < BUTTON_DEBOUNCE_INTERVAL_MS) {
    return;
  }
  last_millis = millis();
  btn = BUTTON_B;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (window_open) {
    xSemaphoreGiveFromISR(button_semaphore, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken == pdTRUE) {
      portYIELD_FROM_ISR();
    }
  }
}

void button_init()
{
  pinMode(buttonA, INPUT);
  pinMode(buttonB, INPUT);
  attachInterrupt(buttonA, buttonAPressed, FALLING);
  attachInterrupt(buttonB, buttonBPressed, FALLING);
  button_semaphore = xSemaphoreCreateBinary();
  last_millis = millis();
}

enum button_type get_user_input(bool block)
{
  window_open = true;
  if (xSemaphoreTake(button_semaphore, block ? portMAX_DELAY : BUTTON_TIMEOUT) == pdTRUE) {
    window_open = false;
    return btn;
  }
  window_open = false;
  return BUTTON_NONE;
}

void read_buttons(int *btnA, int *btnB)
{
  if (btnA != NULL)
    *btnA = digitalRead(buttonA);
  if (btnB != NULL)
    *btnB = digitalRead(buttonB);
}
