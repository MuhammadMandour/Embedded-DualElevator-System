# Collaborative Dual-Elevator System (SPI & Proteus)

## 1. Proteus Component List
Exact names to search in Proteus library:
- STM32F401VE (x2)
- BUTTON (x19 Master, x9 Slave)
- LED-RED (x2, motor simulation)
- LED-YELLOW (x8, floor indicators, optional)
- RES (330 ohm, x2 for motor LEDs)
- VIRTUAL TERMINAL (x1, connected to Master UART)
- LM016L or LCD 16x2 (x2, one per board)
- Power/Ground terminals

## 2. SPI Bus (Master ↔ Slave)
| Signal | Master Pin | Slave Pin |
|--------|-----------|-----------|
| SCK    | PA5       | PA5       |
| MOSI   | PA7       | PA7       |
| MISO   | PA6       | PA6       |
| CS     | PA4       | PA4       |
| GND    | GND       | GND       |
**Common GND is mandatory — floating ground causes corrupted SPI frames.**

## 3. Button Wiring (both boards)
All buttons: Pin → Button → GND. Internal PULL_UP means no external resistor needed.
Master: PB0–PB3 (cabin), PB4 (emergency), PC0–PC5 (hallway) = 11 buttons
Slave:  PB0–PB3 (cabin), PB4 (emergency) = 5 buttons

## 4. Floor Sensor Wiring (both boards)
PA0–PA3: same wiring as buttons — Pin → Button/Switch → GND, PULL_UP active.

## 5. Motor LED Wiring (both boards)
PB6 → 330Ω resistor → LED-RED anode → LED cathode → GND
PWM duty controls brightness: 0%=off, 20%=dim, 100%=full bright.

## 6. LCD Wiring (both boards, 4-bit mode)
| LCD Pin | Board Pin | Notes          |
|---------|-----------|----------------|
| RS      | PE11      |                |
| EN      | PE12      |                |
| D4      | PE7       |                |
| D5      | PE8       |                |
| D6      | PE9       |                |
| D7      | PE10      |                |
| VSS/RW  | GND       | RW tied to GND |
| VDD     | 3.3V      |                |
| V0      | GND       | Max contrast   |

## 7. UART Virtual Terminal
Master PA9 (TX) → Virtual Terminal RX pin.
Baud: 115200, 8N1. No RX wire needed for telemetry-only output.

## 8. Common Mistakes
- Missing common GND between boards → SPI clock reference floating → corrupted frames
- PA6 configured as PWM → destroys SPI MISO line → all RX frames invalid
- CS pin left floating on Slave → random EXTI triggers → phantom SPI bursts
- Timer_Init(TIMER4) called in main → conflicts with Pwm_Init ownership → PWM stops
- Timer_DelayMsAsync(TIMER3,...) called from two places → door timer corrupted
- volatile on uint8_t array → per-byte loads on every access → critical section defeated
