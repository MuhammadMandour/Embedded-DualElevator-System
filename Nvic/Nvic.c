#include "Nvic.h"

#include "stm32f4xx.h"

void Nvic_EnableIrq(uint8 IrqNumber) {
    NVIC_EnableIRQ((IRQn_Type)IrqNumber);
}

void Nvic_DisableIrq(uint8 IrqNumber) {
    NVIC_DisableIRQ((IRQn_Type)IrqNumber);
}
