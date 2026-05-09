/**
 * Spi.c
 *
 *  Created on: Wed May 24 2023
 *  Author    : Abdullah Darwish
 */
#include "stm32f401xe.h"
#include "../Gpio/Gpio.h"
#include "../Nvic/Nvic.h"
#include "Spi.h"

void Spi1_Init(uint8 MasterSlave, uint8 ClkPol, uint8 ClkPhase) {

  Gpio_Init(GPIO_A, 5, GPIO_AF , GPIO_PUSH_PULL);
  Gpio_Init(GPIO_A, 6, GPIO_AF , GPIO_PUSH_PULL);
  Gpio_Init(GPIO_A, 7, GPIO_AF , GPIO_PUSH_PULL);

  
  Gpio_SetAF(GPIO_A, 5, GPIO_AF5);
  Gpio_SetAF(GPIO_A, 6, GPIO_AF5);
  Gpio_SetAF(GPIO_A, 7, GPIO_AF5);
  

  if (MasterSlave == SPI_MASTER) {
    SPI1->CR1 |= (0x1 << SPI_CR1_SSM_Pos);
    SPI1->CR1 |= (0x1 << SPI_CR1_SSI_Pos);
  } else {
    Gpio_Init(GPIO_A, 4, GPIO_AF , GPIO_PUSH_PULL);
    Gpio_SetAF(GPIO_A, 4, GPIO_AF5);
    SPI1->CR1 &= ~(0x1 << SPI_CR1_SSM_Pos);
  }

  SPI1->CR1 &= ~(1 << SPI_CR1_MSTR_Pos);
  SPI1->CR1 |= (MasterSlave << SPI_CR1_MSTR_Pos);

  SPI1->CR1 &= ~(1 << SPI_CR1_CPOL_Pos);
  SPI1->CR1 |= (ClkPol << SPI_CR1_CPOL_Pos);

  SPI1->CR1 &= ~(1 << SPI_CR1_CPHA_Pos);
  SPI1->CR1 |= (ClkPhase << SPI_CR1_CPHA_Pos);

  /*************************************************************************/
  // Baud Rate
  SPI1->CR1 &= ~(0x7 << SPI_CR1_BR_Pos);
  SPI1->CR1 |= (0x5 << SPI_CR1_BR_Pos);  // 16/64 -> 250 kHz, stable in Proteus
  /*************************************************************************/

  SPI1->CR1 |= (1 << SPI_CR1_SPE_Pos);
}

uint8 Spi1_TransmitReceiveByte(uint8 TxData, uint8* RxData) {
  if (SPI1->SR & (1 << SPI_SR_TXE_Pos)) {
    SPI1->DR = TxData;
    while (SPI1->SR & (1 << SPI_SR_BSY_Pos));
    *RxData = SPI1->DR;
    return SPI_OK;
  }
  return SPI_NOK;
}

static uint8* Spi_TxBuf;
static uint8* Spi_RxBuf;
static uint8 Spi_Len;
static uint8 Spi_RxIdx;
static uint8 Spi_TxIdx;
static void (*Spi_Callback)(void) = 0;

void Spi1_StartAsync(uint8* tx_buf, uint8* rx_buf, uint8 len, void (*callback)(void)) {
    if (len == 0) return;
    Spi_TxBuf = tx_buf;
    Spi_RxBuf = rx_buf;
    Spi_Len = len;
    Spi_Callback = callback;
    Spi_RxIdx = 0;
    Spi_TxIdx = 0;

    /* Pre-load the first byte */
    SPI1->DR = Spi_TxBuf[Spi_TxIdx++];

    /* Enable RXNE interrupt */
    SPI1->CR2 |= (1 << SPI_CR2_RXNEIE_Pos);
    Nvic_EnableIrq(35); /* SPI1_IRQn */
}

void SPI1_IRQHandler(void) {
    if (SPI1->SR & (1 << SPI_SR_RXNE_Pos)) {
        Spi_RxBuf[Spi_RxIdx++] = SPI1->DR;
        if (Spi_RxIdx < Spi_Len) {
            if (Spi_TxIdx < Spi_Len) {
                SPI1->DR = Spi_TxBuf[Spi_TxIdx++];
            } else {
                SPI1->DR = 0xFF; /* Dummy byte if needed */
            }
        } else {
            /* Transfer complete */
            SPI1->CR2 &= ~(1 << SPI_CR2_RXNEIE_Pos);
            if (Spi_Callback) Spi_Callback();
        }
    }
}
