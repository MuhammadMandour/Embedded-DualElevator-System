# Collaborative Dual-Elevator System (SPI & Proteus)

This README is intentionally Proteus-specific: it is the simulation wiring guide
for the project, not a replacement for the source code or final report.

## 1. Proteus Component List
Exact names to search in Proteus library:
- STM32F401VE (x2)
- BUTTON (x15 Master, x9 Slave)
- LED-RED (x2, motor simulation)
- RES (330 ohm, x2 for motor LEDs)
- VIRTUAL TERMINAL (x1, connected to Master UART)
- Power/Ground terminals

## 2. SPI Bus (Master to Slave)
| Signal | Master Pin | Slave Pin |
|--------|------------|-----------|
| SCK    | PA5        | PA5       |
| MOSI   | PA7        | PA7       |
| MISO   | PA6        | PA6       |
| CS/NSS | PA4        | PA4       |
| GND    | GND        | GND       |

Common GND is mandatory. Floating ground causes corrupted SPI frames.

## 3. Master Input Wiring
All buttons/sensors are wired as: pin to button/switch to GND. Internal pull-up is enabled in firmware.

Each input uses a unique EXTI line. Do not wire multiple same-numbered pins such as `PA0`, `PB0`, and `PC0` as separate EXTI inputs on the same MCU; STM32 maps only one GPIO port to each EXTI line.

| Function | Pin |
|----------|-----|
| Floor sensors F1-F4 | PA0-PA3 |
| Emergency stop | PB4 |
| Cabin buttons F1-F4 | PB8-PB11 |
| Hall U1 | PC5 |
| Hall D2 | PC6 |
| Hall U2 | PC7 |
| Hall D3 | PC12 |
| Hall U3 | PC13 |
| Hall D4 | PC14 |

## 4. Slave Input Wiring
| Function | Pin |
|----------|-----|
| Floor sensors F1-F4 | PA0-PA3 |
| Emergency stop | PB4 |
| Cabin buttons F1-F4 | PB8-PB11 |

## 5. Motor LED Wiring
PB6 to 330 ohm resistor to LED-RED anode, LED cathode to GND.

PWM duty controls brightness:
- 0% = stop/off
- 20% = slow/dim
- 100% = full speed/full brightness

## 6. UART Virtual Terminal
Master PA9 TX to Virtual Terminal RX.

Baud: 115200, 8N1.

## 7. PWM Math
Timer clock assumed: 16 MHz.

`PWM frequency = 16 MHz / ((PSC + 1) * (ARR + 1))`

Current firmware uses `PSC=15`, `ARR=99`:

`16 MHz / (16 * 100) = 10 kHz`

## 8. SPI Packet Definition
Fixed frame length: 8 bytes.

| Byte | Meaning |
|------|---------|
| 0 | Header `0xA5` |
| 1 | FSM state |
| 2 | Current floor |
| 3 | Target floor |
| 4 | Direction: 0 none, 1 up, 2 down |
| 5 | Request mask: bit0 F1 through bit3 F4 |
| 6 | Flags: bit0 emergency, bit1 door open, bit2 SPI alive |
| 7 | XOR checksum of bytes 0-6 |

## 9. Common Mistakes
- Missing common GND between boards corrupts SPI frames.
- PB6 is PWM; PA6 must stay SPI MISO.
- CS/NSS must connect PA4 to PA4.
- Timer4 belongs to PWM; do not reuse it for delays.
- Timer3 is used for the door timeout.
