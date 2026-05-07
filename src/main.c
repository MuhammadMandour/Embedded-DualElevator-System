/*
 * main.c
 *
 *  Created on: Feb 18, 2025
 *  Author: AbdullahDarwish
 */

#include <string.h>

#include "Rcc.h"
#include "stm32f401xe.h"
#include "Usart.h"

#define RX_Blocking

#ifdef RX_Blocking

int main(void)
{
    Rcc_Init();
    Rcc_Enable(RCC_GPIOA);
    Rcc_Enable(RCC_USART1);
    
    Usart1_Init();
    
    uint8 data = ' ';
    while (1)
    {
        data = Usart1_ReceiveByte();
        
        if (data == '1')
        {
            Usart1_TransmitString("Hello\r\n");
        }else if (data == '2')
        {
            Usart1_TransmitString("World\r\n");
        }
    }
    
    return 0;
}

#else
int main(void)
{
    Rcc_Init();
    Rcc_Enable(RCC_GPIOA);
    Rcc_Enable(RCC_USART1);

    Usart1_Init();


    char cmd_buffer[64];

    while (1)
    {
        char* cmd = Usart1_ReadLine(cmd_buffer, sizeof(cmd_buffer));
        if (cmd != NULL)
        {
            // Echo the command (optional)
            Usart1_TransmitString("Echo>");
            Usart1_TransmitString(cmd);
            Usart1_TransmitString("\r\n");

            // Simple command parsing
            if (strcmp(cmd, "LED_ON") == 0)
            {
                // LED_On();
                Usart1_TransmitString("LED turned ON\r\n");
            }
            else if (strcmp(cmd, "LED_OFF") == 0)
            {
                // LED_Off();
                Usart1_TransmitString("LED turned OFF\r\n");
            }
            else
            {
                Usart1_TransmitString("Unknown command\r\n");
            }
        }
        // Perform other tasks without blocking
        
        
    
    }
}
#endif
