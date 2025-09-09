#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

#define TFT_CS         27
#define TFT_RST        14                        
#define TFT_DC         4

// static Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
// Adafruit_ST7735(int8_t cs, int8_t dc, int8_t mosi, int8_t sclk, int8_t rst);
static Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

static TimerHandle_t TFTTimer;
static int TFTTimeoutMs = 5000;

void TFTTimerCallback(TimerHandle_t)
{
  tft.fillScreen(ST77XX_BLACK);
}

void tftsetup()
{
  tft.initR(INITR_BLACKTAB);      // Init ST7735S chip, black tab.
  tft.fillScreen(ST77XX_BLACK);
  // Switch to landscape mode.
  tft.setRotation(1);
  TFTTimer = xTimerCreate(
    "TFTTimer",
    pdMS_TO_TICKS(TFTTimeoutMs),
    pdFALSE,
    NULL,
    TFTTimerCallback
  );
}

void tftOrderReceived(const char *orderId)
{
  tft.setTextWrap(false);
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(ST77XX_RED);
  tft.setTextSize(2, 2);
  tft.println("Got order!");
  tft.println(orderId);
  tft.println("Proceed?");
  tft.println("A - Yes");
  tft.println("B - No");
}

void tftOrderCancelled()
{
  tft.setTextWrap(false);
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(ST77XX_RED);
  tft.setTextSize(2, 2);
  tft.println("Order");
  tft.println("Cancelled!");
  xTimerStart(TFTTimer, 0);
}

void tftSystemCheck()
{
  tft.setTextWrap(false);
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(ST77XX_RED);
  tft.setTextSize(2, 2);
  tft.println("Performing");
  tft.println("system checks");
}

void tftSysCheckFailed(const char *reason)
{
  tft.println("Failure:");
  tft.println(reason);
  tft.println("Retry?");
  tft.println("A - Yes");
  tft.println("B - No");
}

void tftSysCheckPassed(const char *message)
{
  tft.println(message);
}

void tftDispenseComplete()
{
  tft.println("Finished");
  tft.println("dispensing.");
  tft.println();
  xTimerStart(TFTTimer, 0);
}
