#ifndef LCD_H
#define LCD_H

#include "../Lib/Std_Types.h"

/**
 * @brief Initialize the LCD in 4-bit mode.
 */
void lcd_init(void);

/**
 * @brief Send a command to the LCD.
 * @param cmd The command byte to send.
 */
void lcd_send_command(uint8 cmd);

/**
 * @brief Send a data byte to the LCD.
 * @param data The data byte to send.
 */
void lcd_send_data(uint8 data);

/**
 * @brief Send a string to be displayed on the LCD.
 * @param str Null-terminated string.
 */
void lcd_send_string(char* str);

/**
 * @brief Set the cursor to a specific row and column.
 * @param row 0 for first row, 1 for second row.
 * @param col Column index (0 to 15).
 */
void lcd_set_cursor(uint8 row, uint8 col);

/**
 * @brief Clear the LCD display.
 */
void lcd_clear(void);

#endif /* LCD_H */
