#include <WiFi.h>
#include "wifi_manager.h"

extern const char *ssid;
extern const char *pwd;
extern bool wifiConnected;
extern bool isWifiConnecting;
extern bool timeSynced;

void wifiConnect()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pwd);
  WiFi.setAutoReconnect(true);
  isWifiConnecting = true;
}

void wifiDisconnect()
{
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  wifiConnected = false;
}

void onWiFiConnected()
{
  wifiConnected = true;
  isWifiConnecting = false;
  Serial.println("✅ WiFi Connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  timeSynced = false; // force time resync in loop();
}

void handleWifi()
{
  bool isCurrentlyConnected = (WiFi.status() == WL_CONNECTED);

  if (isCurrentlyConnected && !wifiConnected)
    onWiFiConnected();

  if (!isCurrentlyConnected && wifiConnected)
  {
    Serial.println("❌ WiFi Connection Lost");
    wifiConnected = false;
    timeSynced = false;
  }
}