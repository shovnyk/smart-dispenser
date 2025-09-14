#include "Peripherals.h"

// Inputs (sensors).
static int float_switch = 13;
static int IR_sensor = 25;
static int flow_sensor = 32;

// Outputs (actuator: motor through low side driver MOSFET and buzzer).
static int motor = 33;
static int buzzer = 26;

static uint32_t pulse_count;

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
  // attachInterrupt(flow_sensor, pulse_counter_isr, FALLING);

  // Outputs.
  pinMode(motor, OUTPUT);
  pinMode(buzzer, OUTPUT);
  digitalWrite(motor, LOW);
  digitalWrite(buzzer, LOW);
}

int system_check(int attempt_number)
{
  if (digitalRead(float_switch) == 0) {
    Serial.println("Tank empty!");
    return 1;
  }
  if (digitalRead(IR_sensor) == 1) {
    Serial.println("No container!");
    return 2;
  }
  // if (attempt_number > 0 && (pulse_count == 0)) {
  //   Serial.println("No flow!");
  //   return 3;
  // }
  return 0;
}

void dispense(bool start, int *quantity)
{
  (void)quantity;
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
}
