#include "Peripherals.h"
#include "ZScoreFilter.h"
#include "Button.h"

// Inputs (sensors).
static int float_switch = 13;
static int IR_sensor = 25;
static int flow_sensor = 32;

// Outputs (actuator: motor through low side driver MOSFET and buzzer).
static int motor = 33;
static int buzzer = 26;

static uint32_t pulse_count;
static float calibration_factor = 0.03194F;

static void pulse_counter_isr()
{
  pulse_count++;
}

void peripherals_init()
{
  // Inputs.
  pinMode(float_switch, INPUT);
  pinMode(IR_sensor, INPUT);
  pinMode(flow_sensor, INPUT);
  attachInterrupt(flow_sensor, pulse_counter_isr, FALLING);

  // Outputs.
  pinMode(motor, OUTPUT);
  pinMode(buzzer, OUTPUT);
  digitalWrite(motor, LOW);
  digitalWrite(buzzer, LOW);
}

int system_check(int attempt_number)
{
  int cancel_button = 0;
  if (digitalRead(float_switch) == 0) {
    Serial.println("Tank empty!");
    return 1;
  }
  if (digitalRead(IR_sensor) == 1) {
    Serial.println("No container!");
    return 2;
  }
  // As a manual override - allow user to press cancel button to pause dispensing.
  read_buttons(NULL, &cancel_button);
  if (cancel_button == 0) {
    Serial.println("User pressed cancel button!");
    return 3;
  }
  // if (attempt_number > 0 && (pulse_count == 0)) {
  //   Serial.println("No flow!");
  //   return 4;
  // }
  return 0;
}

void dispense(bool start, int *quantity)
{
  // Guard variable for idempotence.
  static bool dispensing = false;
  if (start && !dispensing) {
    Serial.println("Starting dispense");
    digitalWrite(motor, HIGH);
    dispensing = true;
  }
  else if (!start && dispensing) {
    Serial.println("Stopping dispense");
    digitalWrite(motor, LOW);
    dispensing = false;
  }
  else if (dispensing && (quantity != nullptr)) {
    disableInterrupt(flow_sensor);
    *quantity = *quantity - (z_score_filter(pulse_count) * calibration_factor);
    pulse_count = 0;
    attachInterrupt(flow_sensor, pulse_counter_isr, FALLING);
  }
}
