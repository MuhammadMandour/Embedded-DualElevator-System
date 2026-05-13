/* === REGISTER MAP ===
   RCC:    0x40023800 | GPIOA: 0x40020000 | GPIOB: 0x40020400
   GPIOC:  0x40020800 | GPIOD: 0x40020C00 | GPIOE: 0x40021000
   EXTI:   0x40013C00 | SYSCFG: 0x40013800
   TIM2:   0x40000000 | TIM3:  0x40000400
   TIM4:   0x40000800 | TIM5:  0x40000C00
   SPI1:   0x40013000 | USART1: 0x40011000
   NVIC:   0xE000E100
*/

#include <stdio.h>
#include "../Critical/critical.h"
#include "../Elevator/elevator.h"
#include "../SpiIpc/spi_ipc.h"
#include "../Rcc/Rcc.h"
#include "../Gpio/Gpio.h"
#include "../Exti/Exti.h"
#include "../Timer/Timer.h"
#include "../Pwm/Pwm.h"
#include "../Spi/Spi.h"
#include "../Usart/Usart.h"
#include "../Nvic/Nvic.h"
#include "../Lib/Bit_Operations.h"

volatile uint8_t exti_triggered[16] = {0};
volatile uint8_t spi_rx_ready      = 0;
volatile uint8_t spi_timeout_flag  = 0;
volatile uint8_t door_timeout_flag = 0;

ElevatorContext_t elev_b;
uint8_t slave_tx_packet[8];
uint8_t slave_rx_packet[8];

void EXTI0_Callback(void)  { exti_triggered[0]  = 1; }
void EXTI1_Callback(void)  { exti_triggered[1]  = 1; }
void EXTI2_Callback(void)  { exti_triggered[2]  = 1; }
void EXTI3_Callback(void)  { exti_triggered[3]  = 1; }

void EXTI4_Callback(void) {
    if (Gpio_ReadPin(GPIO_B, 4) == LOW) {
        exti_triggered[4] = 1;
    }
}
void EXTI8_Callback(void)  { exti_triggered[8]  = 1; }
void EXTI9_Callback(void)  { exti_triggered[9]  = 1; }
void EXTI10_Callback(void) { exti_triggered[10] = 1; }
void EXTI12_Callback(void) { exti_triggered[12] = 1; }

void SpiSlaveComplete_Callback(void) { spi_rx_ready   = 1; }
void DoorTimer_Callback(void)        { door_timeout_flag = 1; }
void SpiWatchdog_Callback(void)      { spi_timeout_flag  = 1; }

/* ----------------------------------------------------------------------- */
void Peripheral_Init(void) {
    Rcc_Init();
    Rcc_Enable(RCC_GPIOA); Rcc_Enable(RCC_GPIOB);
    Rcc_Enable(RCC_GPIOD);
    Rcc_Enable(RCC_TIM2);  Rcc_Enable(RCC_TIM3);
    Rcc_Enable(RCC_TIM4);  Rcc_Enable(RCC_TIM5);
    Rcc_Enable(RCC_SPI1);  Rcc_Enable(RCC_USART1); Rcc_Enable(RCC_SYSCFG);

    /* PA0-PA3: floor sensors */
    for (uint8_t i = 0; i <= 3; i++) Gpio_Init(GPIO_A, i, GPIO_INPUT, GPIO_PULL_UP);
    /* PA4-PA7: SPI1 NSS/SCK/MISO/MOSI (slave) */
    Gpio_Init(GPIO_A, 4, GPIO_AF, GPIO_PUSH_PULL); Gpio_SetAF(GPIO_A, 4, GPIO_AF5);
    Gpio_Init(GPIO_A, 5, GPIO_AF, GPIO_PUSH_PULL); Gpio_SetAF(GPIO_A, 5, GPIO_AF5);
    Gpio_Init(GPIO_A, 6, GPIO_AF, GPIO_PUSH_PULL); Gpio_SetAF(GPIO_A, 6, GPIO_AF5);
    Gpio_Init(GPIO_A, 7, GPIO_AF, GPIO_PUSH_PULL); Gpio_SetAF(GPIO_A, 7, GPIO_AF5);
    /* PA9: USART1 TX (slave debug) */
    Gpio_Init(GPIO_A, 9, GPIO_AF, GPIO_PUSH_PULL); Gpio_SetAF(GPIO_A, 9, GPIO_AF7);

    /* PB4: emergency button */
    Gpio_Init(GPIO_B, 4, GPIO_INPUT, GPIO_PULL_UP);
    /* PB6: TIM4 CH1 PWM (motor) */
    Gpio_Init(GPIO_B, 6, GPIO_AF, GPIO_PUSH_PULL); Gpio_SetAF(GPIO_B, 6, GPIO_AF2);
    /* PB8-PB10: cabin buttons F1-F3 */
    for (uint8_t i = 8; i <= 10; i++) Gpio_Init(GPIO_B, i, GPIO_INPUT, GPIO_PULL_UP);
    /* PB12: cabin button F4 */
    Gpio_Init(GPIO_B, 12, GPIO_INPUT, GPIO_PULL_UP);

    Pwm_Init(TIMER4, PWM_CHANNEL_1, 15, 99);
    Pwm_Start(TIMER4, PWM_CHANNEL_1);

    Spi1_Init(SPI_SLAVE, SPI_IDLE_LOW, SPI_SAMPLE_FIRST_TRANSITION);
    Usart1_Init();

    Exti_Init(EXTI_LINE_0,  EXTI_PORT_A, EXTI_EDGE_FALLING, EXTI0_Callback);
    Exti_Init(EXTI_LINE_1,  EXTI_PORT_A, EXTI_EDGE_FALLING, EXTI1_Callback);
    Exti_Init(EXTI_LINE_2,  EXTI_PORT_A, EXTI_EDGE_FALLING, EXTI2_Callback);
    Exti_Init(EXTI_LINE_3,  EXTI_PORT_A, EXTI_EDGE_FALLING, EXTI3_Callback);
    Exti_Init(EXTI_LINE_4,  EXTI_PORT_B, EXTI_EDGE_FALLING, EXTI4_Callback);
    Exti_Init(EXTI_LINE_8,  EXTI_PORT_B, EXTI_EDGE_FALLING, EXTI8_Callback);
    Exti_Init(EXTI_LINE_9,  EXTI_PORT_B, EXTI_EDGE_FALLING, EXTI9_Callback);
    Exti_Init(EXTI_LINE_10, EXTI_PORT_B, EXTI_EDGE_FALLING, EXTI10_Callback);
    Exti_Init(EXTI_LINE_12, EXTI_PORT_B, EXTI_EDGE_FALLING, EXTI12_Callback);

    /* Enable only the lines that were initialised above */
    Exti_Enable(0); Exti_Enable(1); Exti_Enable(2); Exti_Enable(3);
    Exti_Enable(4); Exti_Enable(8); Exti_Enable(9); Exti_Enable(10);
    Exti_Enable(12);

    Nvic_EnableIrq(10); Nvic_EnableIrq(6); Nvic_EnableIrq(7);
    Nvic_EnableIrq(8);  Nvic_EnableIrq(9);

    /* Requirement 5: Emergency EXTI must have highest priority */
    Nvic_SetPriority(10, 0);

    /* Start SPI watchdog — fires if no valid master frame arrives in 250 ms */
    Timer_DelayMsAsync(TIMER5, 250, SpiWatchdog_Callback);
}

/* ----------------------------------------------------------------------- */
static void Process_FloorSensor(uint8_t floor) {
    if (elev_b.current_floor != floor) {
        elev_b.current_floor = floor;
        if (elev_b.current_floor == elev_b.target_floor ||
            READ_BIT(elev_b.request_mask, (floor - 1))) {
            
            elev_b.target_floor = floor;
            Elevator_RunFSM(&elev_b, ELEV_EVENT_FLOOR_REACHED);
            if (elev_b.state == ELEV_DOOR_OPEN) {
                Timer_DelayMsAsync(TIMER3, 3000, DoorTimer_Callback);
            }
        }
    }
}

static void Slave_AddCabinRequest(uint8_t floor) {
    SET_BIT(elev_b.request_mask, (floor - 1));
    if (elev_b.state == ELEV_IDLE || elev_b.state == ELEV_DOOR_OPEN ||
        elev_b.state == ELEV_INDEPENDENT) {
        elev_b.target_floor = floor;
        if (elev_b.current_floor == floor) {
            Elevator_RunFSM(&elev_b, ELEV_EVENT_FLOOR_REACHED);
            Timer_DelayMsAsync(TIMER3, 3000, DoorTimer_Callback);
        }
    }
}

static void Slave_SelectNextCabinTarget(void) {
    if (!(elev_b.state == ELEV_IDLE || elev_b.state == ELEV_DOOR_OPEN ||
          elev_b.state == ELEV_INDEPENDENT)) return;
    if (elev_b.request_mask == 0) return;

    uint8_t best_floor = elev_b.current_floor;
    uint8_t best_diff  = 0xFF;
    for (uint8_t floor = 1; floor <= 4; floor++) {
        if (READ_BIT(elev_b.request_mask, (floor - 1))) {
            uint8_t diff = (floor > elev_b.current_floor)
                           ? (floor - elev_b.current_floor)
                           : (elev_b.current_floor - floor);
            if (diff < best_diff) {
                best_diff  = diff;
                best_floor = floor;
            }
        }
    }
    elev_b.target_floor = best_floor;
}

static void Process_ExtiLine(uint8_t line) {
    switch (line) {
        case 0:  if (Gpio_ReadPin(GPIO_A, 0)  == LOW) Process_FloorSensor(1); break;
        case 1:  if (Gpio_ReadPin(GPIO_A, 1)  == LOW) Process_FloorSensor(2); break;
        case 2:  if (Gpio_ReadPin(GPIO_A, 2)  == LOW) Process_FloorSensor(3); break;
        case 3:  if (Gpio_ReadPin(GPIO_A, 3)  == LOW) Process_FloorSensor(4); break;
        case 4:  if (Gpio_ReadPin(GPIO_B, 4)  == LOW) Elevator_RunFSM(&elev_b, ELEV_EVENT_EMERGENCY_TOGGLE); break;
        case 8:  if (Gpio_ReadPin(GPIO_B, 8)  == LOW) Slave_AddCabinRequest(1); break;
        case 9:  if (Gpio_ReadPin(GPIO_B, 9)  == LOW) Slave_AddCabinRequest(2); break;
        case 10: if (Gpio_ReadPin(GPIO_B, 10) == LOW) Slave_AddCabinRequest(3); break;
        case 12: if (Gpio_ReadPin(GPIO_B, 12) == LOW) Slave_AddCabinRequest(4); break;
        default: break;
    }
}

/* ----------------------------------------------------------------------- */
int main(void) {
    Elevator_InitContext(&elev_b, 1);
    Peripheral_Init();

    /* Pre-load first response frame so SPI is ready before master calls */
    Enter_Critical();
    SPI_PackFrame(&elev_b, slave_tx_packet);
    Spi1_StartAsync(slave_tx_packet, slave_rx_packet, 8, SpiSlaveComplete_Callback);
    Exit_Critical();

    while (1) {
        /* ---- EXTI flag processing ------------------------------------ */
        for (uint8_t line = 0; line <= 12; line++) {
            if (exti_triggered[line]) {
                exti_triggered[line] = 0;
                Process_ExtiLine(line);
            }
        }

        /* ---- Door timeout -------------------------------------------- */
        if (door_timeout_flag) {
            door_timeout_flag = 0;
            Elevator_RunFSM(&elev_b, ELEV_EVENT_DOOR_TIMEOUT);
        }

        /* ---- SPI watchdog timeout ------------------------------------ */
        if (spi_timeout_flag) {
            spi_timeout_flag = 0;
            Elevator_RunFSM(&elev_b, ELEV_EVENT_SPI_TIMEOUT);
            /* Watchdog will be rearmed when next valid frame arrives      */
        }

        /* ---- SPI receive processing ---------------------------------- */
        if (spi_rx_ready) {
            spi_rx_ready = 0;

            if (SPI_ValidateFrame(slave_rx_packet)) {
                elev_b.spi_alive = 1;

                if (elev_b.state == ELEV_INDEPENDENT) {
                    elev_b.state = ELEV_IDLE;
                }

                /* Accept assigned hall calls from master */
                elev_b.request_mask |= slave_rx_packet[5];

                Timer_DelayMsAsync(TIMER5, 250, SpiWatchdog_Callback);
            }
            /* No else: a single bad frame doesn't restart the watchdog —
               it will fire at the scheduled time and trigger INDEPENDENT.  */

            /* Always rearm SPI for the next master transaction regardless
               of whether this frame was valid — the master will keep sending */
            Enter_Critical();
            SPI_PackFrame(&elev_b, slave_tx_packet);
            Spi1_StartAsync(slave_tx_packet, slave_rx_packet, 8, SpiSlaveComplete_Callback);
            Exit_Critical();
        }

        /* ---- Dispatcher & FSM ---------------------------------------- */
        Slave_SelectNextCabinTarget();

        if ((elev_b.state == ELEV_IDLE || elev_b.state == ELEV_DOOR_OPEN ||
             elev_b.state == ELEV_INDEPENDENT) &&
            READ_BIT(elev_b.request_mask, (elev_b.current_floor - 1))) {
            Elevator_RunFSM(&elev_b, ELEV_EVENT_FLOOR_REACHED);
            if (elev_b.state == ELEV_DOOR_OPEN) {
                Timer_DelayMsAsync(TIMER3, 3000, DoorTimer_Callback);
            }
        } else if (elev_b.target_floor != elev_b.current_floor &&
                   (elev_b.state == ELEV_IDLE || elev_b.state == ELEV_DOOR_OPEN ||
                    elev_b.state == ELEV_INDEPENDENT)) {
            Elevator_RunFSM(&elev_b, ELEV_EVENT_TARGET_UPDATED);
            if (elev_b.state == ELEV_DOOR_OPEN) {
                Timer_DelayMsAsync(TIMER3, 3000, DoorTimer_Callback);
            }
        }

        Elevator_UpdateMotor(&elev_b);
    }

    return 0;
}