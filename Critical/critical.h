#ifndef CRITICAL_H
#define CRITICAL_H

#include "cmsis_compiler.h"

#if defined(__GNUC__)
#include "cmsis_gcc.h"
#endif

#define Enter_Critical()  __disable_irq()
#define Exit_Critical()   __enable_irq()

#endif /* CRITICAL_H */
