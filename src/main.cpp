#include <Arduino.h>
#include <DHTesp.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "wifi_manager.h"
#include "lcd_manager.h"
#include "time_manager.h"

/* Pins */
#define TEMP_RELAY_PIN 17
#define RESET_BUTTON_PIN 19
#define DHT22_PIN 23
#define SDA_PIN 21
#define SCL_PIN 22
#define BUZZER_BJT_PIN 5
#define HUMIDIFIER_MOSFET_PIN 18
#define HUMIDIFIER_PAUSE_BUTTON_PIN 4
#define HUMIDIFIER_STATE_LED_PIN 16

/* Global */
DHTesp dhtSensor;
const uint16_t DHT_DELAY = 10 * 1000; // in ms
unsigned long dhtLastRead = 0;        // in ms
float temp = NAN;
float humidity = NAN;
const uint16_t DHT_MAX_TIMEOUT = 2 * 60 * 1000; // in ms
unsigned long lastDhtOkRead = 0; // in ms
bool isSensorOk = false; // flag to indicate if the sensor is OK, used only for LCD display purposes

LiquidCrystal_I2C lcd(0x27, 16, 2); // https://learn.adafruit.com/scanning-i2c-addresses/arduino

// time
unsigned long incubationStartTimestamp = 0; // in seconds

unsigned long NEW_DAY_CHECK_INTERVAL = 30 * 60 * 1000; // in ms
byte currentDay = 0;
unsigned long dayLastCheck = 0; // in ms

unsigned int timeInSeconds = 0;
unsigned long timerLastUpdate = 0; // in ms
byte intervalHours = 0;
unsigned long lastTurnTimestamp = 0;

unsigned long buzzerLastActive = 0; // in ms
const uint16_t BUZZER_DELAY = 1500; // in ms

const unsigned int HUMIDIFIER_PAUSE_MAX_INTERVAL = 5 * 60 * 1000; // in ms
unsigned long humidifierPausedAt = 0;
bool isHumidifierPaused = false;

// wifi connection
const char *ssid;
const char *pwd;
bool isWifiConnecting = false;
bool wifiConnected = false;
bool timeSynced = false;
unsigned long lastSyncAttempt = 0;
const uint16_t SYNC_RETRY_INTERVAL = 30000; // in ms

// config
float earlyTempTarget;
float earlyTempHyst;
float hatchTempTarget;
float hatchTempHyst;
float earlyHumTarget;
float earlyHumHyst;
float hatchHumTarget;
float hatchHumHyst;

float tempTarget;
float tempHyst;
bool heaterState = false;
bool humidifierState = false;
float humidityTarget;
float humidityHyst;

float tempLossPerSecond;
float tempGainPerSecond;
unsigned long heaterLastSwitch = 0; // in ms
float estimatedTemp;

// debounce
const unsigned long DEBOUNCE_DELAY = 50; // 50ms is common

bool lastResetButtonState = HIGH;
bool resetButtonPressed = false;
unsigned long lastResetDebounceTime = 0;

bool lastPauseButtonState = HIGH;
bool pauseButtonPressed = false;
unsigned long lastPauseDebounceTime = 0;

/* Declare Functions */
/**
 * @return whether last read changed or not
 */
bool readSensor();
void updateDynamicConfig();
void setHumidifierState(bool paused);
void writeConfig(StaticJsonDocument<512> &doc);

/* Setup */
void setup()
{
  Serial.begin(115200);

  // pins
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
  pinMode(HUMIDIFIER_PAUSE_BUTTON_PIN, INPUT_PULLUP);

  pinMode(TEMP_RELAY_PIN, OUTPUT);
  digitalWrite(TEMP_RELAY_PIN, LOW);

  pinMode(BUZZER_BJT_PIN, OUTPUT);
  digitalWrite(BUZZER_BJT_PIN, LOW);

  pinMode(HUMIDIFIER_MOSFET_PIN, OUTPUT);
  digitalWrite(HUMIDIFIER_MOSFET_PIN, LOW);

  pinMode(HUMIDIFIER_STATE_LED_PIN, OUTPUT);
  digitalWrite(HUMIDIFIER_STATE_LED_PIN, LOW);

  // file system
  Serial.println("Reading Flash Memory..");
  LittleFS.begin();

  File configFile = LittleFS.open("/config.json", FILE_READ);
  if (!configFile)
  {
    Serial.println("Error opening config file");
    return;
  }

  StaticJsonDocument<512> configDoc; /* arduinojson.org/v6/assistant */
  if (deserializeJson(configDoc, configFile) != DeserializationError::Ok)
  {
    Serial.println("Error deserializing config file");
    return;
  }
  configFile.close();

  incubationStartTimestamp = configDoc["incubation_start_date"];
  JsonObject tempConfig = configDoc["temperature"];
  JsonObject humidityConfig = configDoc["humidity"];
  JsonObject failoverConfig = configDoc["failover"];
  JsonObject turningConfig = configDoc["turning"];

  earlyTempTarget = tempConfig["early_days_target"];
  earlyTempHyst = tempConfig["early_days_hysteresis"];
  hatchTempTarget = tempConfig["hatching_days_target"];
  hatchTempHyst = tempConfig["hatching_days_hysteresis"];

  earlyHumTarget = humidityConfig["early_days_target"];
  earlyHumHyst = humidityConfig["early_days_hysteresis"];
  hatchHumTarget = humidityConfig["hatching_days_target"];
  hatchHumHyst = humidityConfig["hatching_days_hysteresis"];

  tempLossPerSecond = failoverConfig["temp_loss_per_second"];
  tempGainPerSecond = failoverConfig["temp_gain_per_second"];

  byte turnsPerDay = turningConfig["turns_per_day"];
  intervalHours = 24 / turnsPerDay;
  lastTurnTimestamp = turningConfig["last_turn_time"];

  timeInSeconds = intervalHours * 3600; // initial

  File wifiFile = LittleFS.open("/wifi.json", FILE_READ);
  if (!wifiFile)
  {
    Serial.println("Error opening wifi file");
    return;
  }

  StaticJsonDocument<96> wifiDoc;
  if (deserializeJson(wifiDoc, wifiFile) != DeserializationError::Ok)
  {
    Serial.println("Error deserializing wifi file");
    return;
  }
  wifiFile.close();

  ssid = wifiDoc["ssid"].as<const char *>();
  pwd = wifiDoc["pwd"].as<const char *>();

  Serial.println("✅ Configured");

  // wifi
  Serial.print("Establishing Wifi Connection");
  wifiConnect();
  isWifiConnecting = true;

  // timeout after 10 seconds
  unsigned long wifiConnectStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiConnectStart < 10000)
  {
    delay(200);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    onWiFiConnected();
    syncTime();
  }
  else
  {
    wifiConnected = false;
    Serial.println("❌ WiFi Connection Failed");
    Serial.print("Code: ");
    Serial.println(WiFi.status());
  }
  updateDynamicConfig();

  // I2C Protocol
  Wire.begin(SDA_PIN, SCL_PIN);

  // LCD
  lcd.init();
  lcd.backlight();
  updateLCD();
  timerLastUpdate = millis();

  // dht22 sensor
  dhtSensor.setup(DHT22_PIN, DHTesp::DHT22);
  delay(2100); // initializing delay
  readSensor();
}

/* Loop */
void loop()
{
  // WiFi handling
  handleWifi();

  // NTP and dynamic config handling
  if (wifiConnected && !timeSynced && millis() - lastSyncAttempt >= SYNC_RETRY_INTERVAL)
  {
    syncTime();
    updateDynamicConfig();
    lastSyncAttempt = millis();
  }

  // Handle reset button
  bool readingReset = digitalRead(RESET_BUTTON_PIN);
  if (readingReset != lastResetButtonState)
    lastResetDebounceTime = millis();

  if ((millis() - lastResetDebounceTime) > DEBOUNCE_DELAY)
  {
    if (readingReset == LOW && !resetButtonPressed)
    {
      resetButtonPressed = true;
      timeInSeconds = intervalHours * 3600;
      timerLastUpdate = millis();

      if (currentDay > 21 || !incubationStartTimestamp)
      {
        if (timeSynced)
        {
          File configFile = LittleFS.open("/config.json", FILE_READ);
          StaticJsonDocument<512> configDoc;
          deserializeJson(configDoc, configFile);
          configFile.close();

          currentDay = 1;
          incubationStartTimestamp = getUnixTimestamp();
          lastTurnTimestamp = incubationStartTimestamp;
          configDoc["turning"]["last_turn_time"] = lastTurnTimestamp;
          configDoc["incubation_start_date"] = incubationStartTimestamp;
          writeConfig(configDoc);
        }
        else
        {
          lcdType("Internet Access.", "  Is Required!");
          delay(3000);
          updateLCD();
        }
      }
      else if (currentDay < 18)
      {
        if (timeSynced)
        {
          File configFile = LittleFS.open("/config.json", FILE_READ);
          StaticJsonDocument<512> configDoc;
          deserializeJson(configDoc, configFile);
          configFile.close();

          lastTurnTimestamp = getUnixTimestamp();
          configDoc["turning"]["last_turn_time"] = lastTurnTimestamp;
          writeConfig(configDoc);
        }
        updateLCD();
      }
    }
  }
  else if (readingReset == HIGH)
    resetButtonPressed = false;

  lastResetButtonState = readingReset;

  if (!currentDay || !incubationStartTimestamp || currentDay > 22)
    return;

  // Humidifier functionality Pause/Hold control
  if (humidifierPausedAt && millis() - humidifierPausedAt >= HUMIDIFIER_PAUSE_MAX_INTERVAL)
    setHumidifierState(false);

  if (isHumidifierPaused)
  {
    digitalWrite(HUMIDIFIER_MOSFET_PIN, LOW);
    digitalWrite(HUMIDIFIER_STATE_LED_PIN, HIGH);
  }
  else
    digitalWrite(HUMIDIFIER_STATE_LED_PIN, LOW);

  bool readingPause = digitalRead(HUMIDIFIER_PAUSE_BUTTON_PIN);
  if (readingPause != lastPauseButtonState)
    lastPauseDebounceTime = millis();

  if ((millis() - lastPauseDebounceTime) > DEBOUNCE_DELAY)
  {
    if (readingPause == LOW && !pauseButtonPressed)
    {
      pauseButtonPressed = true;
      setHumidifierState(!isHumidifierPaused);
    }
  }
  else if (readingPause == HIGH)
    pauseButtonPressed = false;

  lastPauseButtonState = readingPause;

  // Day check and update
  if (millis() - dayLastCheck >= NEW_DAY_CHECK_INTERVAL)
  {
    if (timeSynced)
    {
      unsigned long currentTimestamp = getUnixTimestamp();
      if (currentTimestamp == 0)
        return;

      isWifiConnecting = false;
      byte newDay = ((currentTimestamp - incubationStartTimestamp) / (24 * 3600)) + 1;
      if (newDay > currentDay)
      {
        currentDay = newDay;
        updateDynamicConfig();
      }
      dayLastCheck = millis();
      wifiDisconnect();
    }
    else if (!isWifiConnecting)
      wifiConnect();
  }

  if (millis() - dhtLastRead >= DHT_DELAY)
  {
    if (readSensor())
      updateLCD();
    dhtLastRead = millis();
  }

  /// Check if the DHT sensor data is still valid within the maximum allowed timeout
  if (millis() - lastDhtOkRead < DHT_MAX_TIMEOUT)
  {
    isSensorOk = true;
    estimatedTemp = temp;
    // Temperature control logic
    if (heaterState)
    {
      if (temp >= tempTarget + tempHyst)
      {
        heaterState = false;
        digitalWrite(TEMP_RELAY_PIN, LOW);
      }
    }
    else
    {
      if (temp < tempTarget - tempHyst)
      {
        heaterState = true;
        digitalWrite(TEMP_RELAY_PIN, HIGH);
      }
    }

    // Humidity control logic, only if not paused
    if (!isHumidifierPaused)
    {
      if (humidifierState)
      {
        if (humidity >= humidityTarget + humidityHyst)
        {
          humidifierState = false;
          digitalWrite(HUMIDIFIER_MOSFET_PIN, LOW);
        }
      }
      else
      {
        if (humidity < humidityTarget - humidityHyst)
        {
          humidifierState = true;
          digitalWrite(HUMIDIFIER_MOSFET_PIN, HIGH);
        }
      }
    }
  }
  else
  {
    // failsafe mode
    if (heaterState)
    {
      estimatedTemp = estimatedTemp + (tempGainPerSecond * (millis() - heaterLastSwitch) / 1000.0);
      if (estimatedTemp >= tempTarget + tempHyst)
      {
        heaterState = false;
        heaterLastSwitch = millis();
        digitalWrite(TEMP_RELAY_PIN, LOW);
      }
      else
      {
        heaterState = true;
        digitalWrite(TEMP_RELAY_PIN, HIGH);
      }
    }
    else
    {
      estimatedTemp = estimatedTemp - (tempLossPerSecond * (millis() - heaterLastSwitch) / 1000.0);
      if (estimatedTemp < tempTarget - tempHyst)
      {
        heaterState = true;
        heaterLastSwitch = millis();
        digitalWrite(TEMP_RELAY_PIN, HIGH);
      }
      else
      {
        heaterState = false;
        digitalWrite(TEMP_RELAY_PIN, LOW);
      }
    }
    Serial.println("⚠️ Sensor timeout! System in failsafe mode.");
    isSensorOk = false;
  }

  // Early/mid cycle handling
  if (currentDay < 18)
  {
    // Timer update
    if (millis() - timerLastUpdate >= 1000 && digitalRead(RESET_BUTTON_PIN) == HIGH && timeInSeconds != 0)
    {
      timeInSeconds--;
      timerLastUpdate = millis();
      updateLCD();
    }

    // Buzzer alarm
    if (timeSynced && timeInSeconds == 0 && millis() - buzzerLastActive >= BUZZER_DELAY)
    {
      digitalWrite(BUZZER_BJT_PIN, !digitalRead(BUZZER_BJT_PIN));
      buzzerLastActive = millis();
    }
  }
}

bool readSensor()
{
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  bool readChange = false;
  if (!isnan(data.temperature) && !isnan(data.humidity))
  {
    readChange = fabs(temp - data.temperature) > 0.1 || fabs(humidity - data.humidity) > 0.1;
    temp = data.temperature;
    humidity = data.humidity;
    lastDhtOkRead = millis();
  }
  return readChange;
}

void updateDynamicConfig()
{
  if (!timeSynced || currentDay < 18)
  {
    // day 0 Safe fallback: assume early/mid cycle
    tempTarget = earlyTempTarget;
    tempHyst = earlyTempHyst;
    humidityTarget = earlyHumTarget;
    humidityHyst = earlyHumHyst;
  }
  else
  {
    tempTarget = hatchTempTarget;
    tempHyst = hatchTempHyst;
    humidityTarget = hatchHumTarget;
    humidityHyst = hatchHumHyst;
  }
}

void setHumidifierState(bool paused)
{
  if (paused)
  {
    humidifierPausedAt = millis();
    isHumidifierPaused = true;
  }
  else
  {
    humidifierPausedAt = 0;
    isHumidifierPaused = false;
  }
}

void writeConfig(StaticJsonDocument<512> &doc)
{
  File configFileW = LittleFS.open("/config.json", FILE_WRITE);
  serializeJson(doc, configFileW);
  configFileW.close();
}