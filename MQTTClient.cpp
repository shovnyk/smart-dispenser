#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "MQTTClient.h"
#include "timestamp.h"

#define MQTT_TASK_STACKSIZE         2048
#define MQTT_TASK_PRORIRITY         1
#define MQTT_QUEUE_MAX_ITEMS        10
#define MQTT_HEARTBEAT_INTERVAL_MS  5000

#define MAX_DISPENSE_AMT_ML         10000
#define STATUS_REPORT_PERIOD        30000

static WiFiClient espClient;
static PubSubClient client(espClient);

static const char *broker = "broker.hivemq.com";
static uint16_t port = 1883;

#define TOPIC_NAME_BUFF_SIZE 64
static char topic_command[TOPIC_NAME_BUFF_SIZE] = { 0 };
static char topic_response[TOPIC_NAME_BUFF_SIZE] = { 0 };
static char topic_status[TOPIC_NAME_BUFF_SIZE] = { 0 };

static char mqtt_client_id[32];

enum mqtt_event_type {
  COMMAND,
  RESPONSE,
  STATUS
} type;

struct dispenser_mqtt_event {
  enum mqtt_event_type type;
  byte data[MQTT_MAX_PAYLOAD_SIZE];
};

struct dispenser_command {
  int qty_mL;
  char order_id[32];
  float density;
};

static QueueHandle_t queue;
static TimerHandle_t heartbeat_timer;

static void mqtt_sub_callback(char* topic, byte* payload, unsigned int length)
{
  BaseType_t xHigherPriorityTaskWoken;
  static struct dispenser_mqtt_event evt;
  if (strncmp(topic, topic_command, min(length, strlen(topic_command))) == 0) {
    evt.type = COMMAND;
    memset(evt.data, 0, sizeof(evt.data));
    memcpy(evt.data, payload, min(sizeof(evt.data), length));
    xQueueSendFromISR(queue, &evt, &xHigherPriorityTaskWoken);
    if( xHigherPriorityTaskWoken ) {
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }
}

static void heartbeat_callback(TimerHandle_t)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  static struct dispenser_mqtt_event evt;
  evt.type = STATUS;
  memset(evt.data, 0, sizeof(evt.data));
  memcpy(evt.data, get_status_report(), sizeof(evt.data));
  xQueueSendFromISR(queue, &evt, &xHigherPriorityTaskWoken);
  if(xHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

static bool mqtt_reconnect()
{
  Serial.print("Attempting MQTT connection...");
  if (client.connect(mqtt_client_id)) {
    Serial.println("connected");
    
    client.publish(topic_status, "Back online.");
    client.subscribe(topic_command);
    return true;
  }
  else {
    return false;
  }
}

static int decode_command(struct dispenser_command* cmd, const byte* pld, uint16_t len)
{
  int arg;
  int err = 0;
  static JsonDocument doc;

  DeserializationError error = deserializeJson(doc, pld, len);
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    mqtt_client_pub(false, "Invalid JSON cmd.");
    return 1;
  }

  auto type = doc["cmd"].as<const char*>();
  if (type == NULL) {
    mqtt_client_pub(false, "No cmd.");
    return 1;
  }

  if (strncmp(type, "DISPENSE", min(strlen("DISPENSE"), strlen(type))) != 0) {
    mqtt_client_pub(false, "Unrecognized cmd.");
    return 1;
  }

  if (!doc["args"]["amt"].is<int>()) {
    mqtt_client_pub(false, "No args.");
    return 1;
  }

  auto amt = doc["args"]["amt"].as<int>();
  if (amt < 0 || amt > MAX_DISPENSE_AMT_ML) {
    mqtt_client_pub(false, "Invalid arg: out of range.");
    return 1;
  }

  if (!doc["args"]["order_id"].is<const char*>()) {
    mqtt_client_pub(false, "No order id.");
    return 1;
  }

  auto order_id = doc["args"]["order_id"].as<const char*>();

  cmd->qty_mL = amt;
  strncpy(cmd->order_id, order_id, sizeof(cmd->order_id));
  // Ignore density for now.
  mqtt_client_pub(true, "Command received.");
  return 0;
}

static void loop(void *pvParameter)
{
  static struct dispenser_mqtt_event evt;
  static struct dispenser_command cmd;

  int err = 0;

  xTimerStart(heartbeat_timer, 0);
  while (true)
  {
    if (!client.connected()) {
      bool success = mqtt_reconnect();
      delay(500);
      if (!success)
        continue;
    }
    client.loop();
    if (xQueueReceive(queue, &evt, pdMS_TO_TICKS(100)) == pdTRUE) { // Avoid starving other threads.
      switch (evt.type)
      {
      case COMMAND:
        err = decode_command(&cmd, evt.data, strlen(reinterpret_cast<char*>(evt.data)));
        if (err) {
          Serial.println("Ignoring command.");
          continue;
        }
        Serial.printf("Order[%s]: %d mL\n", cmd.order_id, cmd.qty_mL);
        break;

      case RESPONSE:
        client.publish(topic_response, reinterpret_cast<char*>(evt.data));
        break;

      case STATUS:
        client.publish(topic_status, reinterpret_cast<char*>(evt.data));
        break;
      }
    }
  }
}

void mqtt_client_setup(const char *device_id)
{
  snprintf(topic_command, sizeof(topic_command), "dispenser/%s/command", device_id);
  snprintf(topic_response, sizeof(topic_command), "dispenser/%s/response", device_id);
  snprintf(topic_status, sizeof(topic_command), "dispenser/%s/status", device_id);
  snprintf(mqtt_client_id, sizeof(mqtt_client_id), "ESP-%s", device_id);

  time_setup();
  client.setServer(broker, port);
  client.setCallback(mqtt_sub_callback);
  heartbeat_timer = xTimerCreate("heartbeat",
    pdMS_TO_TICKS(STATUS_REPORT_PERIOD),
    pdTRUE,
    NULL,
    heartbeat_callback
  );
  queue = xQueueCreate(MQTT_QUEUE_MAX_ITEMS, sizeof(struct dispenser_mqtt_event));
  xTaskCreate(loop, "mqtt_task", MQTT_TASK_STACKSIZE, NULL, MQTT_TASK_PRORIRITY, NULL);
}

void mqtt_client_pub(bool status, const char *message)
{
  static struct dispenser_mqtt_event evt;
  static JsonDocument doc;
  
  evt.type = RESPONSE;
  doc["status"] = status;
  doc["message"] = message;
  serializeJson(doc, evt.data);
  xQueueSend(queue, &evt, 0);
}

void mqtt_client_pub(int qty_dispensed)
{
  static struct dispenser_mqtt_event evt;
  static JsonDocument doc;

  evt.type = RESPONSE;
  doc["status"] = true;
  doc["qty_dispensed_mL"] = qty_dispensed;
  serializeJson(doc, evt.data);
  xQueueSend(queue, &evt, 0);
}
