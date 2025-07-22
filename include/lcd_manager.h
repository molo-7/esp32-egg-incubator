#ifndef LCD_MANAGER_H
#define LCD_MANAGER_H

/**
 * Converts a time duration in seconds to a formatted string (HH:MM:SS).
 * 
 * @return A pointer to a statically allocated string representing the formatted time.
 */
char* formatTimer();


/**
 * Formats current temperature, target temperature, and humidity into a string
 * suitable for LCD display. The format is "xx.x/xx.xC xx.x%".
 * @return A pointer to a statically allocated 17 character string.
 */
char* formatSensorMsg();



/**
 * Displays two lines of text on the LCD.
 *
 * @param line1 The text to display on the first row of the LCD. If empty, the row is not updated.
 * @param line2 The text to display on the second row of the LCD. If empty, the row is not updated.
 */
void lcdType(const char* line1, const char* line2);

/**
 * Updates the LCD display with the current temperature, target temperature, and
 * humidity as well as the current incubation day and a turning timer if
 * applicable. If the incubation cycle is complete, it displays "Incubation Ended"
 * and if there is no internet connection, it displays " Internet Error". If the
 * current day is greater than 22, it displays "     Idle.." and "Press Btn Start!".
 */
void updateLCD();

#endif