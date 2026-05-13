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
#include "../Dispatcher/dispatcher.h"
#include "../SpiIpc/spi_ipc.h"
#include "../Rcc/Rcc.h"
#include "../Gpio/Gpio.h"
#include "../Exti/Exti.h"
#include "../Timer/Timer.h"
#include "../Pwm/Pwm.h"
#include "../Spi/Spi.h"
#include "../Usart/Usart.h"
#include "../Nvic/Nvic.h"

volatile uint8_t exti_triggered[16] = {0};
volatile uint8_t spi_trigger_flag   = 0;
volatile uint8_t spi_rx_ready       = 0;
volatile uint8_t spi_busy           = 0;
volatile uint8_t door_timeout_flag  = 0;
volatile uint8_t uart_trigger_flag  = 0;

volatile uint8_t spi_fault_flag    = 0;
volatile uint8_t spi_fault_counter = 0;

ElevatorContext_t elev_a;
ElevatorContext_t elev_b;
uint8_t master_tx_packet[8];
uint8_t slave_rx_packet[8];

void EXTI0_Callback(void)  { exti_triggered[0]  = 1; }
void EXTI1_Callback(void)  { exti_triggered[1]  = 1; }
void EXTI2_Callback(void)  { exti_triggered[2]  = 1; }
void EXTI3_Callback(void)  { exti_triggered[3]  = 1; }
void EXTI4_Callback(void)  { exti_triggered[4]  = 1; }
void EXTI5_Callback(void)  { exti_triggered[5]  = 1; }
void EXTI6_Callback(void)  { exti_triggered[6]  = 1; }
void EXTI7_Callback(void)  { exti_triggered[7]  = 1; }
void EXTI8_Callback(void)  { exti_triggered[8]  = 1; }
void EXTI9_Callback(void)  { exti_triggered[9]  = 1; }
void EXTI10_Callback(void) { exti_triggered[10] = 1; }
void EXTI12_Callback(void) { exti_triggered[12] = 1; }
void EXTI13_Callback(void) { exti_triggered[13] = 1; }
void EXTI14_Callback(void) { exti_triggered[14] = 1; }

void SpiTimer_Callback(void) { spi_trigger_flag = 1; }

void SpiMasterComplete_Callback(void) {
    Gpio_WritePin(GPIO_A, 4, HIGH);   /* deassert CS */
    spi_busy    = 0;
    spi_rx_ready = 1;
}

void DoorTimer_Callback(void) { door_timeout_flag = 1; }
void UartTimer_Callback(void) { uart_trigger_flag = 1; }

/* ----------------------------------------------------------------------- */
void Peripheral_Init(void) {
    Rcc_Init();
    Rcc_Enable(RCC_GPIOA); Rcc_Enable(RCC_GPIOB);
    Rcc_Enable(RCC_GPIOC); Rcc_Enable(RCC_GPIOD);
    Rcc_Enable(RCC_TIM2);  Rcc_Enable(RCC_TIM3);
    Rcc_Enable(RCC_TIM4);  Rcc_Enable(RCC_TIM5);
    Rcc_Enable(RCC_SPI1);  Rcc_Enable(RCC_USART1); Rcc_Enable(RCC_SYSCFG);

    Usart1_Init();
    Usart1_TransmitString("UART OK\r\n");

    /* PA0-PA3: floor sensor inputs */
    for (uint8_t i = 0; i <= 3; i++) Gpio_Init(GPIO_A, i, GPIO_INPUT, GPIO_PULL_UP);
    /* PA4: SPI CS (output, default HIGH) */
    Gpio_Init(GPIO_A, 4, GPIO_OUTPUT, GPIO_PUSH_PULL);
    Gpio_WritePin(GPIO_A, 4, HIGH);
    //
    // /* PD2: LED Output */
    // Gpio_Init(GPIO_D, 2, GPIO_OUTPUT, GPIO_PUSH_PULL);
    // Gpio_WritePin(GPIO_D, 2, LOW);
    /* PA5-PA7: SPI1 SCK/MISO/MOSI */
    Gpio_Init(GPIO_A, 5, GPIO_AF, GPIO_PUSH_PULL); Gpio_SetAF(GPIO_A, 5, GPIO_AF5);
    Gpio_Init(GPIO_A, 6, GPIO_AF, GPIO_PUSH_PULL); Gpio_SetAF(GPIO_A, 6, GPIO_AF5);
    Gpio_Init(GPIO_A, 7, GPIO_AF, GPIO_PUSH_PULL); Gpio_SetAF(GPIO_A, 7, GPIO_AF5);
    /* PA9-PA10: USART1 TX/RX */
    Gpio_Init(GPIO_A, 9,  GPIO_AF, GPIO_PUSH_PULL); Gpio_SetAF(GPIO_A, 9,  GPIO_AF7);
    Gpio_Init(GPIO_A, 10, GPIO_AF, GPIO_PUSH_PULL); Gpio_SetAF(GPIO_A, 10, GPIO_AF7);

    /* PB4: emergency button */
    Gpio_Init(GPIO_B, 4, GPIO_INPUT, GPIO_PULL_UP);
    /* PB6: TIM4 CH1 PWM (motor) */
    Gpio_Init(GPIO_B, 6, GPIO_AF, GPIO_PUSH_PULL); Gpio_SetAF(GPIO_B, 6, GPIO_AF2);
    /* PB8-PB10: cabin buttons F1-F3 */
    for (uint8_t i = 8; i <= 10; i++) Gpio_Init(GPIO_B, i, GPIO_INPUT, GPIO_PULL_UP);
    /* PB12: cabin button F4 (polled — shares EXTI12 with PC12) */
    Gpio_Init(GPIO_B, 12, GPIO_INPUT, GPIO_PULL_UP);

    /* PC5-PC7, PC12-PC14: hall call buttons */
    for (uint8_t i = 5;  i <= 7;  i++) Gpio_Init(GPIO_C, i, GPIO_INPUT, GPIO_PULL_UP);
    for (uint8_t i = 12; i <= 14; i++) Gpio_Init(GPIO_C, i, GPIO_INPUT, GPIO_PULL_UP);

    Pwm_Init(TIMER4, PWM_CHANNEL_1, 15, 99);
    Pwm_Start(TIMER4, PWM_CHANNEL_1);

    Spi1_Init(SPI_MASTER, SPI_IDLE_LOW, SPI_SAMPLE_FIRST_TRANSITION);

    /* EXTI initialisation — only lines that have a defined callback */
    Exti_Init(EXTI_LINE_0,  EXTI_PORT_A, EXTI_EDGE_FALLING, EXTI0_Callback);
    Exti_Init(EXTI_LINE_1,  EXTI_PORT_A, EXTI_EDGE_FALLING, EXTI1_Callback);
    Exti_Init(EXTI_LINE_2,  EXTI_PORT_A, EXTI_EDGE_FALLING, EXTI2_Callback);
    Exti_Init(EXTI_LINE_3,  EXTI_PORT_A, EXTI_EDGE_FALLING, EXTI3_Callback);
    Exti_Init(EXTI_LINE_4,  EXTI_PORT_B, EXTI_EDGE_FALLING, EXTI4_Callback);
    Exti_Init(EXTI_LINE_5,  EXTI_PORT_C, EXTI_EDGE_FALLING, EXTI5_Callback);
    Exti_Init(EXTI_LINE_6,  EXTI_PORT_C, EXTI_EDGE_FALLING, EXTI6_Callback);
    Exti_Init(EXTI_LINE_7,  EXTI_PORT_C, EXTI_EDGE_FALLING, EXTI7_Callback);
    Exti_Init(EXTI_LINE_8,  EXTI_PORT_B, EXTI_EDGE_FALLING, EXTI8_Callback);
    Exti_Init(EXTI_LINE_9,  EXTI_PORT_B, EXTI_EDGE_FALLING, EXTI9_Callback);
    Exti_Init(EXTI_LINE_10, EXTI_PORT_B, EXTI_EDGE_FALLING, EXTI10_Callback);
    Exti_Init(EXTI_LINE_12, EXTI_PORT_C, EXTI_EDGE_FALLING, EXTI12_Callback);
    Exti_Init(EXTI_LINE_13, EXTI_PORT_C, EXTI_EDGE_FALLING, EXTI13_Callback);
    Exti_Init(EXTI_LINE_14, EXTI_PORT_C, EXTI_EDGE_FALLING, EXTI14_Callback);

    /* FIX Bug 6: enable only the lines that were actually initialised above.
       Original loop 0-14 also enabled line 11 which has no Exti_Init / callback,
       risking a spurious or unhandled interrupt.                                 */
    const uint8_t enabled_lines[] = {0,1,2,3,4,5,6,7,8,9,10,12,13,14};
    for (uint8_t i = 0; i < sizeof(enabled_lines); i++) {
        Exti_Enable(enabled_lines[i]);
    }

    Nvic_EnableIrq(6);  Nvic_EnableIrq(7);  Nvic_EnableIrq(8);
    Nvic_EnableIrq(9);  Nvic_EnableIrq(10); Nvic_EnableIrq(23);
    Nvic_EnableIrq(40);

    /* Requirement 5: Emergency EXTI (EXTI4 on PB4) must have highest priority */
    Nvic_SetPriority(10, 0);

    Timer_DelayMsAsync(TIMER5, 50,  SpiTimer_Callback);
    Timer_DelayMsAsync(TIMER2, 500, UartTimer_Callback);
}

/* ----------------------------------------------------------------------- */
static void Process_FloorSensor(uint8_t floor) {
    if (elev_a.current_floor != floor) {
        elev_a.current_floor = floor;
        if (elev_a.current_floor == elev_a.target_floor || 
            (elev_a.request_mask & ((uint32_t)1 << (floor - 1)))) {
            
            elev_a.target_floor = floor;
            Elevator_RunFSM(&elev_a, ELEV_EVENT_FLOOR_REACHED);
            if (elev_a.state == ELEV_DOOR_OPEN) {
                Timer_DelayMsAsync(TIMER3, 3000, DoorTimer_Callback);
            }
        }
    }
}

static void Master_AddCabinRequest(uint8_t floor) {
    /* FIX Bug 1: use (uint32_t)1 to avoid shift UB on 8/16-bit targets */
    elev_a.request_mask |= ((uint32_t)1 << (floor - 1));
    if (elev_a.state == ELEV_IDLE || elev_a.state == ELEV_DOOR_OPEN) {
        elev_a.target_floor = floor;
        if (elev_a.current_floor == floor) {
            Elevator_RunFSM(&elev_a, ELEV_EVENT_FLOOR_REACHED);
            Timer_DelayMsAsync(TIMER3, 3000, DoorTimer_Callback);
        }
    }
}

static void Master_SelectNextCabinTarget(void) {
    if (!(elev_a.state == ELEV_IDLE || elev_a.state == ELEV_DOOR_OPEN)) return;
    if (elev_a.request_mask == 0) return;

    uint8_t best_floor = elev_a.current_floor;
    uint8_t best_diff  = 0xFF;
    for (uint8_t floor = 1; floor <= 4; floor++) {
        /* FIX Bug 1: same cast here */
        if (elev_a.request_mask & ((uint32_t)1 << (floor - 1))) {
            uint8_t diff = (floor > elev_a.current_floor)
                           ? (floor - elev_a.current_floor)
                           : (elev_a.current_floor - floor);
            if (diff < best_diff) {
                best_diff  = diff;
                best_floor = floor;
            }
        }
    }
    elev_a.target_floor = best_floor;
}

static void Process_ExtiLine(uint8_t line) {
    switch (line) {
        case 0:  if (Gpio_ReadPin(GPIO_A, 0)  == LOW) { Usart1_TransmitString("Floor sensor F1\r\n"); Process_FloorSensor(1); } break;
        case 1:  if (Gpio_ReadPin(GPIO_A, 1)  == LOW) { Usart1_TransmitString("Floor sensor F2\r\n"); Process_FloorSensor(2); } break;
        case 2:  if (Gpio_ReadPin(GPIO_A, 2)  == LOW) { Usart1_TransmitString("Floor sensor F3\r\n"); Process_FloorSensor(3); } break;
        case 3:  if (Gpio_ReadPin(GPIO_A, 3)  == LOW) { Usart1_TransmitString("Floor sensor F4\r\n"); Process_FloorSensor(4); } break;
        case 4:
            if (Gpio_ReadPin(GPIO_B, 4) == LOW) {
                Usart1_TransmitString("Emergency\r\n");
                Elevator_RunFSM(&elev_a, ELEV_EVENT_EMERGENCY_TOGGLE);
            }
            break;
        case 5:  if (Gpio_ReadPin(GPIO_C, 5)  == LOW) { Usart1_TransmitString("Hall U1\r\n"); Dispatcher_AddCall(1, 1); } break;
        case 6:  if (Gpio_ReadPin(GPIO_C, 6)  == LOW) { Usart1_TransmitString("Hall D2\r\n"); Dispatcher_AddCall(2, 2); } break;
        case 7:  if (Gpio_ReadPin(GPIO_C, 7)  == LOW) { Usart1_TransmitString("Hall U2\r\n"); Dispatcher_AddCall(2, 1); } break;
        case 8:  if (Gpio_ReadPin(GPIO_B, 8)  == LOW) { Usart1_TransmitString("Cabin F1\r\n"); Master_AddCabinRequest(1); } break;
        case 9:  if (Gpio_ReadPin(GPIO_B, 9)  == LOW) { Usart1_TransmitString("Cabin F2\r\n"); Master_AddCabinRequest(2); } break;
        case 10: if (Gpio_ReadPin(GPIO_B, 10) == LOW) { Usart1_TransmitString("Cabin F3\r\n"); Master_AddCabinRequest(3); } break;
        case 12: if (Gpio_ReadPin(GPIO_C, 12) == LOW) { Usart1_TransmitString("Hall D3\r\n"); Dispatcher_AddCall(3, 2); } break;
        case 13: if (Gpio_ReadPin(GPIO_C, 13) == LOW) { Usart1_TransmitString("Hall U3\r\n"); Dispatcher_AddCall(3, 1); } break;
        case 14: if (Gpio_ReadPin(GPIO_C, 14) == LOW) { Usart1_TransmitString("Hall D4\r\n"); Dispatcher_AddCall(4, 2); } break;
        default: break;
    }
}

static uint16_t Master_ReadInputMask(void) {
    uint16_t mask = 0;
    for (uint8_t line = 0; line <= 3; line++) {
        if (Gpio_ReadPin(GPIO_A, line) == LOW) mask |= (uint16_t)(1U << line);
    }
    if (Gpio_ReadPin(GPIO_B, 4) == LOW) mask |= (uint16_t)(1U << 4);
    for (uint8_t line = 5; line <= 7; line++) {
        if (Gpio_ReadPin(GPIO_C, line) == LOW) mask |= (uint16_t)(1U << line);
    }
    for (uint8_t line = 8; line <= 10; line++) {
        if (Gpio_ReadPin(GPIO_B, line) == LOW) mask |= (uint16_t)(1U << line);
    }
    if (Gpio_ReadPin(GPIO_C, 12) == LOW) mask |= (uint16_t)(1U << 12);
    if (Gpio_ReadPin(GPIO_C, 13) == LOW) mask |= (uint16_t)(1U << 13);
    if (Gpio_ReadPin(GPIO_C, 14) == LOW) mask |= (uint16_t)(1U << 14);
    return mask;
}

/* ----------------------------------------------------------------------- */
int main(void) {
    Elevator_InitContext(&elev_a, 1);
    Elevator_InitContext(&elev_b, 1);
    Peripheral_Init();

    uint16_t prev_input_mask   = Master_ReadInputMask();
    uint8_t  cabin_f4_was_low  = (Gpio_ReadPin(GPIO_B, 12) == LOW);

    while (1) {
        /* ---- EXTI flag processing ------------------------------------ */
        /* FIX Bug 2 & 3: process EXTI flags first; only fall through to
           GPIO polling if NO flag was pending this cycle.  This prevents
           every event from being dispatched twice (once via flag, once via
           pressed_mask).  prev_input_mask is updated once at the bottom.  */
        uint8_t exti_handled = 0;
        for (uint8_t line = 0; line < 15; line++) {
            if (exti_triggered[line]) {
                exti_triggered[line] = 0;
                Process_ExtiLine(line);
                exti_handled = 1;
            }
        }

        /* ---- Polling fallback (runs only when EXTI was idle) --------- */
        /* Catches any transition that EXTI may have missed (e.g. noise   */
        /* glitch that cleared before ISR fired).                          */
        if (!exti_handled) {
            uint16_t input_mask   = Master_ReadInputMask();
            uint16_t pressed_mask = input_mask & (uint16_t)(~prev_input_mask);
            for (uint8_t line = 0; line < 15; line++) {
                if (pressed_mask & (uint16_t)(1U << line)) {
                    Process_ExtiLine(line);
                }
            }
            prev_input_mask = input_mask;   /* FIX Bug 3: update once, here */
        } else {
            /* Keep prev_input_mask in sync even when EXTI handled events */
            prev_input_mask = Master_ReadInputMask();
        }

        /* ---- PB12 cabin F4 (shares EXTI12 with PC12, polled) --------- */
        uint8_t cabin_f4_is_low = (Gpio_ReadPin(GPIO_B, 12) == LOW);
        if (cabin_f4_is_low && !cabin_f4_was_low) {
            Usart1_TransmitString("Cabin F4\r\n");
            Master_AddCabinRequest(4);
        }
        cabin_f4_was_low = cabin_f4_is_low;

        /* ---- Door timeout -------------------------------------------- */
        if (door_timeout_flag) {
            door_timeout_flag = 0;
            Elevator_RunFSM(&elev_a, ELEV_EVENT_DOOR_TIMEOUT);
        }

        /* ---- SPI periodic transmit ------------------------------------ */
        if (spi_trigger_flag) {
            spi_trigger_flag = 0;

            if (spi_busy) {
                /* FIX Bug 4: cap counter before increment to prevent
                   uint8_t rollover from clearing spi_fault_flag          */
                if (spi_fault_counter < 0xFF) spi_fault_counter++;
                if (spi_fault_counter >= 4)   spi_fault_flag = 1;
            } else {
                /* FIX Bug 5: assert CS inside the critical section so no
                   interrupt can fire between CS-low and Spi1_StartAsync   */
                Enter_Critical();
                SPI_PackFrame(&elev_a, master_tx_packet);
                
                /* Send elev_b's assigned hall calls to the Slave */
                master_tx_packet[5] = elev_b.request_mask;
                uint8_t chk = 0;
                for(uint8_t i = 0; i < 7; i++) chk ^= master_tx_packet[i];
                master_tx_packet[7] = chk;

                Gpio_WritePin(GPIO_A, 4, LOW);   /* assert CS */
                spi_busy = 1;
                Spi1_StartAsync(master_tx_packet, slave_rx_packet, 8, SpiMasterComplete_Callback);
                Exit_Critical();
            }

            Timer_DelayMsAsync(TIMER5, 50, SpiTimer_Callback);
        }

        /* ---- SPI receive processing ----------------------------------- */
        if (spi_rx_ready) {
            spi_rx_ready = 0;
            if (SPI_ValidateFrame(slave_rx_packet)) {
                spi_fault_counter = 0;
                spi_fault_flag    = 0;
                Enter_Critical();
                SPI_UnpackFrame(slave_rx_packet, &elev_b);
                Exit_Critical();
            } else {
                /* FIX Bug 4: same overflow cap here */
                if (spi_fault_counter < 0xFF) spi_fault_counter++;
                if (spi_fault_counter >= 4)   spi_fault_flag = 1;
            }
        }

        /* ---- UART status telemetry ------------------------------------ */
        if (uart_trigger_flag) {
            uart_trigger_flag = 0;
            char buf[80];
            snprintf(buf, sizeof(buf),
                     "[A] St:%d Fl:%d Tg:%d | [B] St:%d Fl:%d Tg:%d | SPI:%s\r\n",
                     elev_a.state, elev_a.current_floor, elev_a.target_floor,
                     elev_b.state, elev_b.current_floor, elev_b.target_floor,
                     spi_fault_flag ? "FAULT" : "OK");
            Usart1_TransmitString(buf);
            Timer_DelayMsAsync(TIMER2, 500, UartTimer_Callback);
        }

        /* ---- Dispatcher & FSM ---------------------------------------- */
        Master_SelectNextCabinTarget();
        Dispatcher_ReevaluateQueue(&elev_a, &elev_b, spi_fault_flag);

        if ((elev_a.state == ELEV_IDLE || elev_a.state == ELEV_DOOR_OPEN) &&
            (elev_a.request_mask & ((uint32_t)1 << (elev_a.current_floor - 1)))) {
            Elevator_RunFSM(&elev_a, ELEV_EVENT_FLOOR_REACHED);
            if (elev_a.state == ELEV_DOOR_OPEN) {
                Timer_DelayMsAsync(TIMER3, 3000, DoorTimer_Callback);
            }
        } else if (elev_a.target_floor != elev_a.current_floor) {
            Elevator_RunFSM(&elev_a, ELEV_EVENT_TARGET_UPDATED);
            if (elev_a.state == ELEV_DOOR_OPEN) {
                Timer_DelayMsAsync(TIMER3, 3000, DoorTimer_Callback);
            }
        }

        Elevator_UpdateMotor(&elev_a);
    }

    return 0;
}