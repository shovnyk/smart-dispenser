#pragma once

#include "MQTTClient.h"

void main_submit();

enum main_event_type {
  MQTT,
  BUTTON,
  FLOW_SENSOR,
  IR_SENSOR,
  FLOAT_SWITCH
};

struct main_event {
  enum main_event_type type;
  union {

  } data;
}