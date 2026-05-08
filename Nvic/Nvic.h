#ifndef NVIC_H
#define NVIC_H

#include "../Lib/Std_Types.h"

void Nvic_EnableIrq(uint8 IrqNumber);
void Nvic_DisableIrq(uint8 IrqNumber);
void Nvic_SetPriority(uint8 IrqNumber, uint8 Priority);

#endif /* NVIC_H */
