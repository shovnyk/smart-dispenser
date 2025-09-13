#pragma once

#define MQTT_MAX_PAYLOAD_SIZE 256

void mqtt_client_setup(const char *);
void mqtt_client_pub(bool status, const char *message);
void mqtt_client_pub(int qty_dispensed);
