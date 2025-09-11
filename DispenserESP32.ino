#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "TTFT.hpp"
#include "NTC.hpp"

// WiFi Credentials.
#include "secrets.h"

// MQTT broker details.
static const char *deviceId;
static const char* mqttBrokerDomain = "broker.hivemq.com";

#define TOPIC_NAME_BUFF 64
static char topicCommand[TOPIC_NAME_BUFF] = "dispenser/command";
static char topicResponse[TOPIC_NAME_BUFF] = "dispenser/response";
static char topicStatus[TOPIC_NAME_BUFF] = "dispenser/status";

WiFiClient espClient;
PubSubClient client(espClient);

// MQTT commands and responses.
#define MAX_DISPENSE_AMT_ML   5000
#define STATUS_REPORT_PERIOD  30000

typedef enum {
  CMD_INVALID,
  CMD_DISPENSE_ORDERED,
  CMD_DISPENSE_PROCEED_NO,
  CMD_SYSTEM_CHECK
} command_type_t;

typedef struct {
  command_type_t type;
  union {
    struct { char id[32]; int amt;  } dispense;
  } args;
} command_t;

static command_t command;

typedef enum {
  CMD_ACK,
  CMD_NACK,
} response_type_t;

typedef struct {
  response_type_t type;
  const char *reason;
} response_t;

static response_t response;

static volatile bool mqtt_rcvd = false;

static volatile bool order_received = false;
static volatile bool dispensing = false;
static volatile bool syscheck_failed = false;
static volatile bool dispensing_done = false;

static JsonDocument progressReport;
static char buff[128];
static int qty;
static int when_to_stop;

static int motorPin = 26;
static int button1 = 34;
static int button2 = 21;
static int floatSwitch = 22;
static int irPin = 5;

static void setResponse(response_t *response, response_type_t type, const char *reason)
{
  response->type = type;
  response->reason = reason;
}

static void callback(char* topic, byte* payload, unsigned int length)
{
  if (strncmp(topic, topicCommand, min(length, strlen(topicCommand))) == 0) {
    decodeCommand(&command, payload, length);
    mqtt_rcvd = true;
  }
}

static int decodeCommand(command_t *cmd, const byte* pld, uint16_t len)
{
  int arg;
  int err = 0;
  static JsonDocument doc;

  cmd->type = CMD_INVALID;
  memset(&cmd->args, 0, sizeof(cmd->args));

  DeserializationError error = deserializeJson(doc, pld, len);
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    setResponse(&response, CMD_NACK, "Invalid JSON cmd.");
    return 1;
  }

  auto type = doc["cmd"].as<const char*>();
  if (type == NULL) {
    setResponse(&response, CMD_NACK, "No cmd.");
    return 1;
  }

  if (strncmp(type, "DISPENSE", min(strlen("DISPENSE"), strlen(type))) != 0) {
    setResponse(&response, CMD_NACK, "Unrecognized cmd.");
    return 1;
  }

  if (!doc["args"]["amt"].is<int>()) {
    setResponse(&response, CMD_NACK, "No args.");
    return 1;
  }

  auto amt = doc["args"]["amt"].as<int>();
  if (amt < 0 || amt > MAX_DISPENSE_AMT_ML) {
    setResponse(&response, CMD_NACK, "Invalid arg: out of range.");
    return 1;
  }

  if (!doc["args"]["order_id"].is<const char*>()) {
    setResponse(&response, CMD_NACK, "No order id.");
    return 1;
  }

  auto orderId = doc["args"]["order_id"].as<const char*>();

  cmd->type = CMD_DISPENSE_ORDERED;
  cmd->args.dispense.amt = amt;
  strncpy(cmd->args.dispense.id, orderId, sizeof(cmd->args.dispense.id));
  setResponse(&response, CMD_ACK, "cmd rcvd.");
  return 0;
}

static void setupWiFi()
{
  Serial.printf("Connecting to %s\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros()); // Seed PRNG for client ID randomness generator. 
  Serial.printf("WiFi connected @%s\n", WiFi.localIP().toString().c_str());
}

static void reconnect()
{
  while (!client.connected())
  {
    Serial.println("Attempting MQTT connection...");
    
    // Create a random client ID.
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    
    // Attempt to connect.
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(topicStatus, "Back online.");
      // ... and resubscribe.
      client.subscribe(topicCommand);
    }
    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

static const char *encodeResponse(const response_t* response)
{
  static JsonDocument doc;
  static char buff[256] = {0};

  doc.clear();
  doc["rsp"] = response->type == CMD_ACK ? "ACK" : "NACK";
  if (response->reason != NULL)
    doc["reason"] =  response->reason;
  doc.shrinkToFit();
  serializeJson(doc, buff);
  return buff;
}

const char *getStatusReport(void)
{
  static char buff[256]; // TODO: same buffer for every publish?
  static JsonDocument doc;
  doc["stat"] = "online";
  doc["ts"] = getTimestamp(); // TODO: make timestamping part of the publish flow.
  doc.shrinkToFit();
  serializeJson(doc, buff);
  return buff;
}

const char* getTimestamp()
{
  static char buff[20];
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);

  if (t) {
      snprintf(buff, sizeof(buff), "%02d:%02d:%02d %02d/%02d/%02d",
              t->tm_hour, t->tm_min, t->tm_sec,
              t->tm_mday, t->tm_mon + 1, t->tm_year % 100);
  } else {
      snprintf(buff, sizeof(buff), "00:00:00 00/00/00");
  }
  return buff;
}

void setupTime()
{
  int ntimes = 2;
  const long gmtOffset_sec = 19800;
  const int daylightOffset_sec = 0;
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");
  struct tm timeinfo;
  bool success = false;
  while (!success && ntimes--) {
    success = getLocalTime(&timeinfo);
    delay(500);
  }
  if (!success) {
    Serial.println("Warning - could not access time - probably WAN issue!");
  }
}

TimerHandle_t dispensingTimer;

void dispenseDoneCallback(TimerHandle_t timer)
{
  dispensing = false;
  dispensing_done = true;
  digitalWrite(motorPin, LOW);
}

void setup()
{
  Serial.begin(115200);

  pinMode(motorPin, OUTPUT);
  pinMode(button1, INPUT);
  pinMode(button2, INPUT);
  pinMode(floatSwitch, INPUT);
  pinMode(irPin, INPUT);
  tftsetup();

  // Turn motor off initially.
  digitalWrite(motorPin, LOW);

  setupWiFi();
  client.setServer(mqttBrokerDomain, 1883);
  client.setCallback(callback);
  
  setupTime();
  Serial.printf("NTC temperature: %.2f\n", ntc.readTemperature());

  // attachInterrupt(floatSwitch, tankPossiblyEmpty, FALLING);
  // attachInterrupt(irPin, containerPossiblyRemoved, FALLING);
}

void loop()
{
  static int last_millis = 0;
  static int dispense_start_millis = 0;
  static int progress_interval_ms = 300;
  static int amtDispensed = 0;

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  static int last_status_report = 0; 
  if (millis() - last_status_report > STATUS_REPORT_PERIOD) {
    client.publish(topicStatus, getStatusReport());
    last_status_report = millis();
  }

#define MAGIC 38.4615 // ms/mL.

  if (mqtt_rcvd) {
    switch(command.type)
    {
      case CMD_DISPENSE_ORDERED:
        Serial.printf("[Order #%s] Dispense: %d mL of liquid.\n", command.args.dispense.id, command.args.dispense.amt); 
        qty = command.args.dispense.amt;
        client.publish(topicResponse, encodeResponse(&response));
        when_to_stop = MAGIC * command.args.dispense.amt;
        progress_interval_ms = min(500, when_to_stop/10);
        Serial.printf("Dispenser for %d ms.\n", when_to_stop);
        tftOrderReceived(command.args.dispense.id); // Ask user if they want to start dispensing...
        order_received = true;
        break;

      case CMD_DISPENSE_PROCEED_NO:
        response.type = CMD_NACK;
        response.reason = "User cancelled order.";
        Serial.println(response.reason);
        client.publish(topicResponse, encodeResponse(&response));
        tftOrderCancelled();
        when_to_stop = 0;
        break;

      case CMD_SYSTEM_CHECK:
        tftSystemCheck();
        if (digitalRead(floatSwitch) == 0) { // No liquid available for dispensing.
          tftSysCheckFailed("Tank empty!");
          syscheck_failed = true;
          break;
        }
        else {
          tftSysCheckPassed(" Liquid OK");
        }
        
        if (digitalRead(irPin) == 1) { // No container available for dispensing.
          tftSysCheckFailed("No container!");
          syscheck_failed = true;
          break;
        }
        else {
          tftSysCheckPassed(" Container OK");
        }

        // Can start dispensing now.
        response.type = CMD_ACK;
        response.reason = "Syscheck passed. Start dispensing.";
        Serial.println(response.reason);
        client.publish(topicResponse, encodeResponse(&response));
        dispensingTimer = xTimerCreate(
          "DispenseTimer",
          pdMS_TO_TICKS(when_to_stop),
          pdFALSE,
          NULL,
          dispenseDoneCallback
        );
        xTimerStart(dispensingTimer, 0);
        dispensing = true;
        amtDispensed = 0;
        last_millis = millis();
        dispense_start_millis = millis();
        digitalWrite(motorPin, HIGH);
        break;

      case CMD_INVALID:
        Serial.println("Invalid command.");
        client.publish(topicResponse, encodeResponse(&response));
      default:
        break;
    }
    mqtt_rcvd = false;
  }

  if (syscheck_failed) {
    if (digitalRead(button2) == 0) { // User does not want to re-check.
      command.type = CMD_DISPENSE_PROCEED_NO;
      mqtt_rcvd = true;
      syscheck_failed = false;
    }
    else if (digitalRead(button1) == 0) { // User wants to re-check.
      command.type = CMD_SYSTEM_CHECK;
      mqtt_rcvd = true;
      syscheck_failed = false;
    }
  }

  if (order_received) {
    if (digitalRead(button2) == 0) { // User has cancelled order.
      command.type = CMD_DISPENSE_PROCEED_NO;
      mqtt_rcvd = true;
      order_received = false;
    }
    else if (digitalRead(button1) == 0) { // User wants to proceed with the order.
      command.type = CMD_SYSTEM_CHECK;
      mqtt_rcvd = true;
      order_received = false;
    }
  }

  char tmp[16] = { 0 };
  if (dispensing && (millis() - last_millis > progress_interval_ms)) {
    last_millis = millis();
    response.type = CMD_ACK;
    amtDispensed = (int)((millis() - dispense_start_millis) / MAGIC);
    snprintf(tmp, sizeof(tmp), "%d mL", amtDispensed);
    response.reason = tmp;
    client.publish(topicResponse, encodeResponse(&response));
  }

  if (dispensing_done) {
    response.type = CMD_ACK;
    response.reason = "Dispensing complete.";
    Serial.println(response.reason);
    client.publish(topicResponse, encodeResponse(&response));
    tftDispenseComplete();
    dispensing_done = false;
  }
}
