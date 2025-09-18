#include <WiFi.h>

#include "MQTTClient.h"
#include "Button.h"
#include "Peripherals.h"
#include "LoadCell.h"
#include "TimedMovingAverageVector.h"
#include "LinearInterpolation.h"
#include "secrets.h"
#include "esp_mac.h"

static char device_id[16] = { 0 };

#define MAIN_QUEUE_MAX_ITEMS 10 // Note: additional orders may be queued up!
static QueueHandle_t queue;

static void device_setup()
{
  uint8_t softApMac[6];
  esp_read_mac(softApMac, ESP_MAC_WIFI_SOFTAP);
  snprintf(device_id, sizeof(device_id), "%02X%02X%02X%02X%02X%02X",
                softApMac[0], softApMac[1], softApMac[2], softApMac[3], softApMac[4], softApMac[5]);

  button_init();
  peripherals_init();
  loadcell_init();
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

void main_submit(int qty) {
  xQueueSend(queue, &qty, 0);
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
  int qty;
  int err;
  const char *rsp;
  int nitr = 0;

  // I. Receive order.
  xQueueReceive(queue, &qty, portMAX_DELAY);

  Serial.println("I. Order received:");
  Serial.println("   a. Proceed: place container and press A.");
  Serial.println("   b. Cancel: press B.");
  enum button_type input = get_user_input(false);
  if (input == BUTTON_B) {
    rsp = "User cancelled order.";
    Serial.println(rsp);
    mqtt_client_pub(false, rsp);
    return;
  }
  if (input == BUTTON_NONE) {
    rsp = "Timed out waiting for user.";
    Serial.println(rsp);
    mqtt_client_pub(false, rsp);
    return;
  }

  int qty_copy = qty;

  struct data_point dp;
  dp.volume_mL = qty;
  interpolate_linear(&dp);
  flow_sensor_calibrate(dp.dispense_factor);
  Serial.printf("Using calibration factor = %f\n", dp.dispense_factor);
  
  loadcell_tare();
  tmav_init();

  // II. System Check and Dispense.  
  while (true)
  {
    err = system_check(nitr++);
    if (err)
    {
      dispense(false, nullptr);
      Serial.println("Failed system check. Do you want to retry?");
      mqtt_client_pub(false, "Syscheck failed. Pausing dispense.");
      input = get_user_input(true);
      // Cannot timeout this time as user may take time to refil/check.
      if (input == BUTTON_B) {
        rsp = "User cancelled order.";
        Serial.println(rsp);
        mqtt_client_pub(false, rsp);
        return;
      }
      tmav_init();
      continue;
    }

    // System check passed.
    dispense(true, &qty);
    vTaskDelay(pdMS_TO_TICKS(100));
    if (qty < 0) {
      dispense(false, nullptr);
      break;
    }

    // float weight_grams;
    // if (tmav_insert(loadcell_get_weight(), &weight_grams)) {
    //   Serial.printf("%.1f grams\n", weight_grams);
    //   mqtt_client_pub(qty_copy - qty); // Amount dispensed.
    // }
  }

  rsp = "Dispense successful!";
  mqtt_client_pub(true, rsp);
}
