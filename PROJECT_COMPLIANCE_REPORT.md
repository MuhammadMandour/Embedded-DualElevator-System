# Project Compliance Report

## Implemented
- Dual firmware targets: Master/Dispatcher/Elevator A and Slave/Elevator B.
- Full-duplex SPI IPC frame: 8 bytes, `0xA5` header, state data, flags, XOR checksum.
- Master-driven dispatcher with immediate, perfect-match, passed-match, opposite-direction rejection, idle-nearest, and communication-fault handling.
- Elevator FSM states: idle, moving up, moving down, door open, emergency, independent.
- Timer-driven 50 ms SPI exchange and 500 ms UART telemetry.
- PWM motor LED simulation at 0%, 20%, and 100%.
- Volatile ISR flags and critical sections around shared SPI/dispatcher buffers.

## Hardware Notes
- Inputs are mapped to unique EXTI lines. This is required because each STM32 EXTI line can select only one GPIO port at a time.
- SPI uses PA5 SCK, PA7 MOSI, PA6 MISO, and PA4 CS/NSS on both boards.
- PWM output uses PB6.
- UART telemetry uses USART1 PA9 TX at 115200 baud.

## Pre-Lab Data
- Register map is documented at the top of `Master/main.c` and `Slave/main.c`.
- PWM math is documented in `README.md`.
- SPI packet definition is documented in `README.md` and `SpiIpc/spi_ipc.h`.
