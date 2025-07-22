#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

/**
 * Synchronizes the device's time with an NTP server.
 * 
 * This function attempts to configure and synchronize the time using NTP servers
 * "pool.ntp.org" and "time.nist.gov". It retries up to 20 times with a 500ms delay
 * between attempts if the initial synchronization fails. If successful, it updates
 * various time-related variables including the current day and time since the last
 * egg turn. If it fails, it marks the device as offline for time synchronization.
 * It also disconnects from WiFi after a successful sync to save power.
 */
void syncTime();

/**
 * @brief Retrieves the current Unix timestamp.
 * 
 * @details Attempts to fetch the current local time and convert it to
 * a Unix timestamp, representing seconds since 00:00:00 UTC, January 1, 1970.
 * If the local time retrieval fails, it sets the time synchronization flag
 * to false and returns 0.
 * 
 * @return Current Unix timestamp in seconds, or 0 if the time is not synchronized.
 */
unsigned long getUnixTimestamp();

#endif