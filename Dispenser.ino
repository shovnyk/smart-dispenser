// Includes.
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>

// WiFi Credentials.
static const char* ssid = "shvnksamsung";
static const char* password = "12345678";

// MQTT broker details.
static const char *deviceId;
static const char* mqttBrokerDomain = "broker.hivemq.com";
#define TOPIC_NAME_BUFF 64
static char topicCommand[TOPIC_NAME_BUFF] = "dispenser/command";
static char topicResponse[TOPIC_NAME_BUFF] = "dispenser/response";
static char topicStatus[TOPIC_NAME_BUFF] = "dispenser/status";

// Globals.
WiFiClient espClient;
PubSubClient client(espClient);

// MQTT commands and responses.
#define MAX_DISPENSE_AMT_ML   5000
#define STATUS_REPORT_PERIOD  30000

typedef enum {
  CMD_INVALID,
  CMD_DISPENSE
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

static void setResponse(response_t *response, response_type_t type, const char *reason)
{
  response->type = type;
  response->reason = reason;
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

  cmd->type = CMD_DISPENSE;
  cmd->args.dispense.amt = amt;
  strncpy(cmd->args.dispense.id, orderId, sizeof(cmd->args.dispense.id));
  setResponse(&response, CMD_ACK, "cmd rcvd.");
  return 0;
}

static volatile bool mqtt_rcvd = false;

static void callback(char* topic, byte* payload, unsigned int length)
{
  if (strncmp(topic, topicCommand, min(length, strlen(topicCommand))) == 0) {
    decodeCommand(&command, payload, length);
    mqtt_rcvd = true;
    // Ideally : parse and put into a thread-safe queue that is dequeued by the main thread.
  }
}

static void setupWiFi()
{
  delay(10);

  // We start by connecting to a WiFi network.
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros()); // Sede PRNG for client ID randomness generator. 

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

static void reconnect()
{
  // Loop until we're reconnected.
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
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
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying.
      delay(5000);
    }
  }
}

void setupTime()
{
  const long gmtOffset_sec = 19800;
  const int daylightOffset_sec = 0;
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    delay(500);
  }
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

#define CALIBRATION_FACTOR  10// 27.776
#define PERIOD              1000

static const int flowSensorPin = 12; // D5 on NodeMCU.
static volatile int pulseCount;  
static float flowRate;
static unsigned int flowMilliLitres;
static unsigned int totalMilliLitres;
static const int motorPin = 14; // D6 on NodeMCU.

IRAM_ATTR void pulseCounter()
{
  // Increment the pulse counter. Needs lock.
  pulseCount++;
}

static int last_millis;

void setup()
{
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(motorPin, OUTPUT);
  pinMode(flowSensorPin, INPUT_PULLUP);

  setupWiFi();
  setupTime();

  // deviceId = WiFi.macAddress().c_str();
  // snprintf(topicCommand, TOPIC_NAME_BUFF, "dispenser/%s/command", deviceId);
  // snprintf(topicResponse, TOPIC_NAME_BUFF, "dispenser/%s/response", deviceId);
  // snprintf(topicStatus, TOPIC_NAME_BUFF, "dispenser/%s/status", deviceId);

  client.setServer(mqttBrokerDomain, 1883);
  client.setCallback(callback);
  
  pulseCount        = 0;
  flowRate          = 0.0;
  flowMilliLitres   = 0;
  totalMilliLitres  = 0;
  attachInterrupt(digitalPinToInterrupt(flowSensorPin), pulseCounter, FALLING);

  last_millis = millis();
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

static volatile bool dispensing = false;

static JsonDocument progressReport;
static char buff[128];
static int qty;
static int when_to_stop;

void loop()
{
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  static int last_status_report = 0; 
  if (millis() - last_status_report > STATUS_REPORT_PERIOD) {
    client.publish(topicStatus, getStatusReport());
    last_status_report = millis();
  }

#define MAGIC 38.4615 // ms/mL

  if (mqtt_rcvd) { // TODO: Race conditions? Best to use a queue!
    switch(command.type)
    {
      case CMD_DISPENSE:
        Serial.printf("[Order #%s] Dispense: %d mL of liquid.\n", command.args.dispense.id, command.args.dispense.amt); 
        qty = command.args.dispense.amt;
        client.publish(topicResponse, encodeResponse(&response));
        pulseCount = 0;
        dispensing = true;
        digitalWrite(motorPin, HIGH);
        when_to_stop = MAGIC * command.args.dispense.amt;
        Serial.printf("Dispenser for %d ms.\n", when_to_stop);
        last_millis = millis();
        // Start dispense sequence.
        break;

      case CMD_INVALID:
        Serial.println("Invalid command.");
        client.publish(topicResponse, encodeResponse(&response));
      default:
        break;
    }
    mqtt_rcvd = false;
  }

  if (dispensing && (millis() - last_millis > when_to_stop)) {
    digitalWrite(motorPin, LOW);
    dispensing = false;
    last_millis = millis();
    Serial.println("Done");
  }

// #define REAL_TIME_MONITORING_INTERVAL 1000
//   if (dispensing && (millis() - last_millis > REAL_TIME_MONITORING_INTERVAL))
//   {
//     last_millis = (micros()/1000);

//     flowRate = pulseCount / (float)CALIBRATION_FACTOR;
//     flowMilliLitres = (flowRate / 60) * 1000;
//     totalMilliLitres += flowMilliLitres;
//     Serial.printf("Dispensed: %d\n", totalMilliLitres);

//     progressReport.clear();
//     progressReport["rsp"] = "in progress";
//     progressReport["mL"] =  totalMilliLitres;
//     progressReport.shrinkToFit();
//     serializeJson(progressReport, buff);
//     client.publish(topicResponse, buff);

//     if (totalMilliLitres >= qty) {
//       digitalWrite(motorPin, LOW);
//       dispensing = false;
//       totalMilliLitres = 0;
//       pulseCount = 0;
//     }
//     pulseCount = 0;
//   }
}
