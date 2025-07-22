#include <Arduino.h>
#include "time_manager.h"
#include "wifi_manager.h"

extern bool timeSynced;
extern unsigned long lastTurnTimestamp;
extern byte intervalHours;
extern byte currentDay;
extern unsigned long incubationStartTimestamp;
extern unsigned long dayLastCheck;
extern int timeInSeconds;
extern unsigned long lastSyncAttempt;
extern bool wifiConnected;
extern bool timeSynced;

void syncTime()
{
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("Waiting for time sync...");

  struct tm timeinfo;
  bool success = getLocalTime(&timeinfo);
  byte attempt = 0;
  while (!success && attempt < 20)
  {
    delay(500);
    attempt++;
    success = getLocalTime(&timeinfo);
  }

  if (!success)
  {
    Serial.println("❌ NTP sync failed. Treating as offline.");
    timeSynced = false;
    wifiConnected = false;
    return;
  }

  unsigned long currentTimestamp = mktime(&timeinfo);
  timeSynced = true;
  Serial.println("✅ Time synced");
  wifiDisconnect();

  float passedHours = (currentTimestamp - lastTurnTimestamp) / 3600.0;
  timeInSeconds = lastTurnTimestamp ? (intervalHours - fmod(passedHours, intervalHours)) * 3600 : intervalHours * 3600;
  currentDay = incubationStartTimestamp ? ((currentTimestamp - incubationStartTimestamp) / (24 * 3600)) + 1 : 0;
   
  dayLastCheck = millis();
  lastSyncAttempt = dayLastCheck;
}

unsigned long getUnixTimestamp()
{
  struct tm currentTime;
  if (!getLocalTime(&currentTime))
  {
    timeSynced = false;
    return 0;
  }
  return mktime(&currentTime);
}