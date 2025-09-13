#include <WiFi.h>

#include "MQTTClient.h"
#include "secrets.h"
#include "esp_mac.h"

static char device_id[16] = { 0 };

#define MAIN_QUEUE_MAX_ITEMS 10
static QueueHandle_t queue;

static void device_setup()
{
  uint8_t softApMac[6];
  esp_read_mac(softApMac, ESP_MAC_WIFI_SOFTAP);
  snprintf(device_id, sizeof(device_id), "%02X%02X%02X%02X%02X%02X",
                softApMac[0], softApMac[1], softApMac[2], softApMac[3], softApMac[4], softApMac[5]);
}

static void wifi_setup()
{
  Serial.printf("Connecting to %s\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("WiFi connected @%s\n", WiFi.localIP().toString().c_str());
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Serial initialized.");

  device_setup();
  Serial.println(device_id);
  wifi_setup();
  mqtt_client_setup(device_id);

  queue = xQueueCreate(MAIN_QUEUE_MAX_ITEMS, sizeof(int));
}

void loop()
{
  if (xQueueReceive(queue, &int, portMAX_DELAY) == pdTRUE) {
    
  }
}

void main_submit(int qty)
{
  xQueueSend(queue, &qty, 0);
}
