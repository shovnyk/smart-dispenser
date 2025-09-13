#pragma once

void time_setup()
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

const char* get_time_stamp()
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

static const char *get_status_report(void)
{
  static char buff[256];
  static JsonDocument doc;
  doc["stat"] = "online";
  doc["ts"] = get_time_stamp();
  doc.shrinkToFit();
  serializeJson(doc, buff);
  return buff;
}
