/**
 * Exti.c
 *
 *  Created on: 5/16/2025
 *  Author    : AbdallahDarwish
 */
#include "Exti.h"
#include "../Lib/Bit_Operations.h"

typedef struct {
    volatile uint32 IMR;
    volatile uint32 EMR;
    volatile uint32 RTSR;
    volatile uint32 FTSR;
    volatile uint32 SWIER;
    volatile uint32 PR;
} ExtiType;

typedef struct {
    volatile uint32 NVIC_ISER[8];
    uint32 _r[24];
    volatile uint32 NVIC_ICER[8];
} NvicType;

typedef struct {
    uint32 MEMRMP;
    uint32 PMC;
    uint32 EXTICR[4];
    uint32 _r[2];
    uint32 CMPCR;
} SyscfgType;


#define EXTI          ((ExtiType*)0x40013C00)
#define NVIC          ((NvicType*)0xE000E100)
#define SYSCFG        ((SyscfgType*)0x40013800)


ExtiCallback ExtiCallbacks[16] = {0};
uint8 ExtiLineNumberNvicMap[16] = {6, 7, 8, 9, 10, 23, 23, 23, 23, 23, 40, 40, 40, 40, 40, 40};

void Exti_Init(uint8 LineNumber, uint8 PortName, uint8 EdgeType, ExtiCallback Callback) {
    uint8 sysConfigIndex = LineNumber / 4;
    uint8 sysConfigLogicalBitPosition = LineNumber % 4;

    SYSCFG->EXTICR[sysConfigIndex] &= ~(0x0f << (sysConfigLogicalBitPosition * 4));
    SYSCFG->EXTICR[sysConfigIndex] |= (PortName << (sysConfigLogicalBitPosition * 4));

    ExtiCallbacks[LineNumber] = Callback;

    CLEAR_BIT(EXTI->RTSR, LineNumber);
    CLEAR_BIT(EXTI->FTSR, LineNumber);

    switch (EdgeType) {
        case EXTI_EDGE_RISING:
            SET_BIT(EXTI->RTSR, LineNumber);
            break;
        case EXTI_EDGE_FALLING:
            SET_BIT(EXTI->FTSR, LineNumber);
            break;
        case EXTI_EDGE_BOTH:
            SET_BIT(EXTI->RTSR, LineNumber);
            SET_BIT(EXTI->FTSR, LineNumber);
            break;
        default:
            break;
    }
}

void Exti_Enable(uint8 LineNumber) {
    SET_BIT(EXTI->IMR, LineNumber);
    uint8 irqNumber = ExtiLineNumberNvicMap[LineNumber];
    SET_BIT(NVIC->NVIC_ISER[irqNumber / 32], (irqNumber % 32));
}

void Exti_Disable(uint8 LineNumber) {
    CLEAR_BIT(EXTI->IMR, LineNumber);
    uint8 irqNumber = ExtiLineNumberNvicMap[LineNumber];
    SET_BIT(NVIC->NVIC_ICER[irqNumber / 32], (irqNumber % 32));
}


void EXTI0_IRQHandler(void) {
    if (ExtiCallbacks[0] != 0) {
        ExtiCallbacks[0]();
    }
    SET_BIT(EXTI->PR, 0); // Clear pending bit
}

void EXTI1_IRQHandler(void) {
    if (ExtiCallbacks[1]) {
        ExtiCallbacks[1]();
    }
    SET_BIT(EXTI->PR, 1); // Clear pending bit
}

void EXTI2_IRQHandler(void) {
    if (ExtiCallbacks[2]) {
        ExtiCallbacks[2]();
    }
    SET_BIT(EXTI->PR, 2); // Clear pending bit
}

void EXTI3_IRQHandler(void) {
    if (ExtiCallbacks[3]) {
        ExtiCallbacks[3]();
    }
    SET_BIT(EXTI->PR, 3); // Clear pending bit
}

void EXTI4_IRQHandler(void) {
    if (ExtiCallbacks[4]) {
        ExtiCallbacks[4]();
    }
    SET_BIT(EXTI->PR, 4); // Clear pending bit
}

void EXTI9_5_IRQHandler(void) {
    if (READ_BIT(EXTI->PR, 5)) {
        if (ExtiCallbacks[5]) {
            ExtiCallbacks[5]();
        }
        SET_BIT(EXTI->PR, 5); // Clear pending bit
    }
    if (READ_BIT(EXTI->PR, 6)) {
        if (ExtiCallbacks[6]) {
            ExtiCallbacks[6]();
        }
        SET_BIT(EXTI->PR, 6); // Clear pending bit
    }
    if (READ_BIT(EXTI->PR, 7)) {
        if (ExtiCallbacks[7]) {
            ExtiCallbacks[7]();
        }
        SET_BIT(EXTI->PR, 7); // Clear pending bit
    }
    if (READ_BIT(EXTI->PR, 8)) {
        if (ExtiCallbacks[8]) {
            ExtiCallbacks[8]();
        }
        SET_BIT(EXTI->PR, 8); // Clear pending bit
    }
    if (READ_BIT(EXTI->PR, 9)) {
        if (ExtiCallbacks[9]) {
            ExtiCallbacks[9]();
        }
        SET_BIT(EXTI->PR, 9); // Clear pending bit
    }
}

void EXTI15_10_IRQHandler(void) {
    if (READ_BIT(EXTI->PR, 10)) {
        if (ExtiCallbacks[10]) {
            ExtiCallbacks[10]();
        }
        SET_BIT(EXTI->PR, 10); // Clear pending bit
    }
    if (READ_BIT(EXTI->PR, 11)) {
        if (ExtiCallbacks[11]) {
            ExtiCallbacks[11]();
        }
        SET_BIT(EXTI->PR, 11); // Clear pending bit
    }
    if (READ_BIT(EXTI->PR, 12)) {
        if (ExtiCallbacks[12]) {
            ExtiCallbacks[12]();
        }
        SET_BIT(EXTI->PR, 12); // Clear pending bit
    }
    if (READ_BIT(EXTI->PR, 13)) {
        if (ExtiCallbacks[13]) {
            ExtiCallbacks[13]();
        }
        SET_BIT(EXTI->PR, 13); // Clear pending bit
    }
    if (READ_BIT(EXTI->PR, 14)) {
        if (ExtiCallbacks[14]) {
            ExtiCallbacks[14]();
        }
        SET_BIT(EXTI->PR, 14); // Clear pending bit
    }
    if (READ_BIT(EXTI->PR, 15)) {
        if (ExtiCallbacks[15]) {
            ExtiCallbacks[15]();
        }
        SET_BIT(EXTI->PR, 15); // Clear pending bit
    }
}
