#ifndef CRITICAL_H
#define CRITICAL_H

#include "cmsis_compiler.h"

#define Enter_Critical()  __disable_irq()
#define Exit_Critical()   __enable_irq()

#endif /* CRITICAL_H */
