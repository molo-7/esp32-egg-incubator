#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "lcd_manager.h"

extern LiquidCrystal_I2C lcd;
extern byte currentDay;
extern float temp;
extern float tempTarget;
extern float humidity;
extern int timeInSeconds;
extern bool timeSynced;
extern unsigned long incubationStartTimestamp;
extern bool isSensorOk;

char *formatTimer()
{
  uint8_t hours = timeInSeconds / 3600;
  uint8_t minutes = (timeInSeconds % 3600) / 60;
  uint8_t seconds = timeInSeconds % 60;

  static char buffer[9];
  sprintf(buffer, "%02u:%02u:%02u", hours, minutes, seconds);

  return buffer;
}

char *formatSensorMsg()
{
  static char buffer[17];

  sprintf(
      buffer,
      "%.1f/%.1fC %.1f%%", // 16 Chars
      temp,
      tempTarget,
      humidity
    );

  return buffer;
}

void updateLCD()
{
  if (!incubationStartTimestamp || currentDay > 22)
    return lcdType("     Idle..", "Press Btn Start!");

  char buffer[17];
  if (!timeSynced)
    sprintf(buffer, " Internet Error"); // indicates no internet connection
  else if (currentDay == 22)
    sprintf(buffer, "Incubation Ended"); // Incubation cycle ended
  else if (currentDay > 18)
    sprintf(buffer, "%02u/21d Hatching!", currentDay); // hatching days no turning timer
  else
    sprintf(buffer, "%02u/21d %s", currentDay, formatTimer()); // turning timer

  lcdType(isSensorOk ? formatSensorMsg() : "Sensor Timeout!!", buffer);
}

void lcdType(const char *line1, const char *line2)
{
  if (strlen(line1) > 0) {
    lcd.setCursor(0, 0);
    lcd.print(line1);
  }

  if (strlen(line2) > 0) {
    lcd.setCursor(0, 1);
    lcd.print(line2);
  }
}