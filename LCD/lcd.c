#include "lcd.h"
#include "Gpio.h"

/* ===================== LCD Pin Configuration ===================== */

#define LCD_PORT        GPIO_B

#define LCD_RS_PIN      0
#define LCD_EN_PIN      1

#define LCD_D4_PIN      2
#define LCD_D5_PIN      10
#define LCD_D6_PIN      9
#define LCD_D7_PIN      12

/* ===================== LCD Commands ===================== */

#define LCD_CMD_CLEAR           0x01
#define LCD_CMD_HOME            0x02
#define LCD_CMD_FUNCTION_SET    0x28
#define LCD_CMD_DISPLAY_ON      0x0C
#define LCD_CMD_ENTRY_MODE      0x06

#define LCD_LINE1_ADDR          0x80
#define LCD_LINE2_ADDR          0xC0

/* ===================== Timing ===================== */

#define LCD_POWER_ON_DELAY_MS   50
#define LCD_CLEAR_DELAY_MS      2

/* ===================== Delay Functions ===================== */

static void delay_us(uint32 us)
{
    for (volatile uint32 i = 0; i < us * 4; i++)
    {
        __asm("NOP");
    }
}

static void delay_ms(uint32 ms)
{
    while (ms--)
    {
        delay_us(1000);
    }
}

/* ===================== Low-Level Functions ===================== */

static void lcd_pulse_enable(void)
{
    Gpio_WritePin(LCD_PORT, LCD_EN_PIN, HIGH);
    delay_us(2);
    Gpio_WritePin(LCD_PORT, LCD_EN_PIN, LOW);
    delay_us(100);
}

static void lcd_send_nibble(uint8 nibble)
{
    Gpio_WritePin(LCD_PORT, LCD_D4_PIN, (nibble >> 0) & 0x01);
    Gpio_WritePin(LCD_PORT, LCD_D5_PIN, (nibble >> 1) & 0x01);
    Gpio_WritePin(LCD_PORT, LCD_D6_PIN, (nibble >> 2) & 0x01);
    Gpio_WritePin(LCD_PORT, LCD_D7_PIN, (nibble >> 3) & 0x01);

    lcd_pulse_enable();
}

/* ===================== Public Functions ===================== */

void lcd_send_command(uint8 cmd)
{
    Gpio_WritePin(LCD_PORT, LCD_RS_PIN, LOW);

    lcd_send_nibble(cmd >> 4);
    lcd_send_nibble(cmd & 0x0F);

    if (cmd == LCD_CMD_CLEAR || cmd == LCD_CMD_HOME)
    {
        delay_ms(LCD_CLEAR_DELAY_MS);
    }
}

void lcd_send_data(uint8 data)
{
    Gpio_WritePin(LCD_PORT, LCD_RS_PIN, HIGH);

    lcd_send_nibble(data >> 4);
    lcd_send_nibble(data & 0x0F);
}

void lcd_init(void)
{
    /* Configure Pins */
    Gpio_Init(LCD_PORT, LCD_RS_PIN, GPIO_OUTPUT, GPIO_PUSH_PULL);
    Gpio_Init(LCD_PORT, LCD_EN_PIN, GPIO_OUTPUT, GPIO_PUSH_PULL);

    Gpio_Init(LCD_PORT, LCD_D4_PIN, GPIO_OUTPUT, GPIO_PUSH_PULL);
    Gpio_Init(LCD_PORT, LCD_D5_PIN, GPIO_OUTPUT, GPIO_PUSH_PULL);
    Gpio_Init(LCD_PORT, LCD_D6_PIN, GPIO_OUTPUT, GPIO_PUSH_PULL);
    Gpio_Init(LCD_PORT, LCD_D7_PIN, GPIO_OUTPUT, GPIO_PUSH_PULL);

    Gpio_WritePin(LCD_PORT, LCD_RS_PIN, LOW);
    Gpio_WritePin(LCD_PORT, LCD_EN_PIN, LOW);

    delay_ms(LCD_POWER_ON_DELAY_MS);

    /* Initialization sequence (4-bit mode) */
    lcd_send_nibble(0x03);
    delay_ms(5);

    lcd_send_nibble(0x03);
    delay_us(150);

    lcd_send_nibble(0x03);
    lcd_send_nibble(0x02); // 4-bit mode

    /* Configuration */
    lcd_send_command(LCD_CMD_FUNCTION_SET);
    lcd_send_command(LCD_CMD_DISPLAY_ON);
    lcd_send_command(LCD_CMD_ENTRY_MODE);
    lcd_send_command(LCD_CMD_CLEAR);
}

void lcd_send_string(char *str)
{
    while (*str)
    {
        lcd_send_data((uint8)*str++);
    }
}

void lcd_set_cursor(uint8 row, uint8 col)
{
    uint8 address;

    switch (row)
    {
        case 0:
            address = LCD_LINE1_ADDR + col;
            break;
        case 1:
            address = LCD_LINE2_ADDR + col;
            break;
        default:
            return;
    }

    lcd_send_command(address);
}

void lcd_clear(void)
{
    lcd_send_command(LCD_CMD_CLEAR);
}