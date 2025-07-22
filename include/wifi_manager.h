#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

/**
 * @brief Tries to connect to WiFi using the stored SSID and password.
 *
 * @details
 * Sets WiFi mode to WIFI_STA, begins the connection with the stored SSID
 * and password, performs a network scan, and enables auto reconnect.
 * Sets `isWifiConnecting` to true. Call this function to start a connection
 * attempt.
 */
void wifiConnect();

/**
 * Disconnects from WiFi and sets the WiFi mode to OFF, disabling the interface.
 *
 * @details
 * When called, it sets `wifiConnected` to false.
 */
void wifiDisconnect();

/**
 * Callback function triggered when the device successfully connects to WiFi.
 *
 * @details
 * This function updates the WiFi connection status flags, outputs the
 * connection status and local IP address to the serial monitor, and sets
 * the time synchronization flag to false to ensure the time is resynced
 * in the main loop.
 */

void onWiFiConnected();

/**
 * @brief Checks WiFi status and calls onWiFiConnected() if status has
 *        changed to connected, or handles disconnection.
 *
 * @details This function is meant to be called in the main loop.
 */
void handleWifi();

#endif