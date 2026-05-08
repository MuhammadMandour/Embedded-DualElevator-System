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
#include "../LCD/lcd.h"

volatile uint8_t exti_triggered[16] = {0};
volatile uint8_t spi_trigger_flag = 0;
volatile uint8_t spi_rx_ready = 0;
volatile uint8_t spi_busy = 0;
volatile uint8_t door_timeout_flag = 0;
volatile uint8_t uart_trigger_flag = 0;

volatile uint8_t spi_fault_flag = 0, spi_fault_counter = 0;

ElevatorContext_t elev_a;
ElevatorContext_t elev_b;
uint8_t master_tx_packet[8];
uint8_t slave_rx_packet[8];

void EXTI0_Callback(void) { exti_triggered[0] = 1; }
void EXTI1_Callback(void) { exti_triggered[1] = 1; }
void EXTI2_Callback(void) { exti_triggered[2] = 1; }
void EXTI3_Callback(void) { exti_triggered[3] = 1; }
void EXTI4_Callback(void) { exti_triggered[4] = 1; }
void EXTI5_Callback(void) { exti_triggered[5] = 1; }
void EXTI6_Callback(void) { exti_triggered[6] = 1; }
void EXTI7_Callback(void) { exti_triggered[7] = 1; }
void EXTI8_Callback(void) { exti_triggered[8] = 1; }
void EXTI9_Callback(void) { exti_triggered[9] = 1; }
void EXTI10_Callback(void) { exti_triggered[10] = 1; }
void EXTI11_Callback(void) { exti_triggered[11] = 1; }
void EXTI12_Callback(void) { exti_triggered[12] = 1; }
void EXTI13_Callback(void) { exti_triggered[13] = 1; }
void EXTI14_Callback(void) { exti_triggered[14] = 1; }

void SpiTimer_Callback(void) { spi_trigger_flag = 1; }
void SpiMasterComplete_Callback(void) {
    Gpio_WritePin(GPIO_A, 4, HIGH);
    spi_busy = 0;
    spi_rx_ready = 1;
}
void DoorTimer_Callback(void) { door_timeout_flag = 1; }
void UartTimer_Callback(void) { uart_trigger_flag = 1; }

void Peripheral_Init(void) {
    Rcc_Init();
    Rcc_Enable(RCC_GPIOA); Rcc_Enable(RCC_GPIOB);
    Rcc_Enable(RCC_GPIOC); Rcc_Enable(RCC_GPIOD); Rcc_Enable(RCC_GPIOE);
    Rcc_Enable(RCC_TIM2); Rcc_Enable(RCC_TIM3);
    Rcc_Enable(RCC_TIM4); Rcc_Enable(RCC_TIM5);
    Rcc_Enable(RCC_SPI1); Rcc_Enable(RCC_USART1);

    for(uint8_t i=0; i<=3; i++) Gpio_Init(GPIO_A, i, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_A, 4, GPIO_OUTPUT, GPIO_PUSH_PULL); Gpio_WritePin(GPIO_A, 4, HIGH);
    Gpio_Init(GPIO_A, 5, GPIO_AF, GPIO_PUSH_PULL); Gpio_SetAF(GPIO_A, 5, GPIO_AF5);
    Gpio_Init(GPIO_A, 6, GPIO_AF, GPIO_PUSH_PULL); Gpio_SetAF(GPIO_A, 6, GPIO_AF5);
    Gpio_Init(GPIO_A, 7, GPIO_AF, GPIO_PUSH_PULL); Gpio_SetAF(GPIO_A, 7, GPIO_AF5);
    Gpio_Init(GPIO_A, 9, GPIO_AF, GPIO_PUSH_PULL); Gpio_SetAF(GPIO_A, 9, GPIO_AF7);
    Gpio_Init(GPIO_A, 10, GPIO_AF, GPIO_PUSH_PULL); Gpio_SetAF(GPIO_A, 10, GPIO_AF7);

    Gpio_Init(GPIO_B, 4, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_B, 6, GPIO_AF, GPIO_PUSH_PULL); Gpio_SetAF(GPIO_B, 6, GPIO_AF2);
    for(uint8_t i=8; i<=11; i++) Gpio_Init(GPIO_B, i, GPIO_INPUT, GPIO_PULL_UP);

    for(uint8_t i=5; i<=7; i++) Gpio_Init(GPIO_C, i, GPIO_INPUT, GPIO_PULL_UP);
    for(uint8_t i=12; i<=14; i++) Gpio_Init(GPIO_C, i, GPIO_INPUT, GPIO_PULL_UP);
    for(uint8_t i=7; i<=12; i++) Gpio_Init(GPIO_E, i, GPIO_OUTPUT, GPIO_PUSH_PULL);

    Pwm_Init(TIMER4, PWM_CHANNEL_1, 15, 99);
    Pwm_Start(TIMER4, PWM_CHANNEL_1);

    Spi1_Init(SPI_MASTER, SPI_IDLE_LOW, SPI_SAMPLE_FIRST_TRANSITION);
    Usart1_Init();
    lcd_init();

    Exti_Init(EXTI_LINE_0, EXTI_PORT_A, EXTI_EDGE_FALLING, EXTI0_Callback);
    Exti_Init(EXTI_LINE_1, EXTI_PORT_A, EXTI_EDGE_FALLING, EXTI1_Callback);
    Exti_Init(EXTI_LINE_2, EXTI_PORT_A, EXTI_EDGE_FALLING, EXTI2_Callback);
    Exti_Init(EXTI_LINE_3, EXTI_PORT_A, EXTI_EDGE_FALLING, EXTI3_Callback);
    Exti_Init(EXTI_LINE_4, EXTI_PORT_B, EXTI_EDGE_FALLING, EXTI4_Callback);
    Exti_Init(EXTI_LINE_5, EXTI_PORT_C, EXTI_EDGE_FALLING, EXTI5_Callback);
    Exti_Init(EXTI_LINE_6, EXTI_PORT_C, EXTI_EDGE_FALLING, EXTI6_Callback);
    Exti_Init(EXTI_LINE_7, EXTI_PORT_C, EXTI_EDGE_FALLING, EXTI7_Callback);
    Exti_Init(EXTI_LINE_8, EXTI_PORT_B, EXTI_EDGE_FALLING, EXTI8_Callback);
    Exti_Init(EXTI_LINE_9, EXTI_PORT_B, EXTI_EDGE_FALLING, EXTI9_Callback);
    Exti_Init(EXTI_LINE_10, EXTI_PORT_B, EXTI_EDGE_FALLING, EXTI10_Callback);
    Exti_Init(EXTI_LINE_11, EXTI_PORT_B, EXTI_EDGE_FALLING, EXTI11_Callback);
    Exti_Init(EXTI_LINE_12, EXTI_PORT_C, EXTI_EDGE_FALLING, EXTI12_Callback);
    Exti_Init(EXTI_LINE_13, EXTI_PORT_C, EXTI_EDGE_FALLING, EXTI13_Callback);
    Exti_Init(EXTI_LINE_14, EXTI_PORT_C, EXTI_EDGE_FALLING, EXTI14_Callback);

    for(uint8_t i=0; i<=14; i++) Exti_Enable(i);

    Nvic_EnableIrq(6); Nvic_EnableIrq(7); Nvic_EnableIrq(8);
    Nvic_EnableIrq(9); Nvic_EnableIrq(10); Nvic_EnableIrq(23);

    /* Requirement 5: Emergency EXTI (EXTI4 on PB4) must have highest priority */
    Nvic_SetPriority(10, 0);

    Timer_DelayMsAsync(TIMER5, 50, SpiTimer_Callback);
    Timer_DelayMsAsync(TIMER2, 500, UartTimer_Callback);
}

static void Process_FloorSensor(uint8_t floor) {
    if (elev_a.current_floor != floor) {
        elev_a.current_floor = floor;
        if (elev_a.current_floor == elev_a.target_floor) {
            Elevator_RunFSM(&elev_a, ELEV_EVENT_FLOOR_REACHED);
            if (elev_a.state == ELEV_DOOR_OPEN) {
                Timer_DelayMsAsync(TIMER3, 3000, DoorTimer_Callback);
            }
        }
    }
}

static void Master_AddCabinRequest(uint8_t floor) {
    elev_a.request_mask |= (1 << (floor - 1));
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
    uint8_t best_diff = 0xFF;
    for (uint8_t floor = 1; floor <= 4; floor++) {
        if (elev_a.request_mask & (1 << (floor - 1))) {
            uint8_t diff = (floor > elev_a.current_floor) ? (floor - elev_a.current_floor) : (elev_a.current_floor - floor);
            if (diff < best_diff) {
                best_diff = diff;
                best_floor = floor;
            }
        }
    }
    elev_a.target_floor = best_floor;
}

static void Process_ExtiLine(uint8_t line) {
    switch (line) {
        case 0: if (Gpio_ReadPin(GPIO_A, 0) == LOW) Process_FloorSensor(1); break;
        case 1: if (Gpio_ReadPin(GPIO_A, 1) == LOW) Process_FloorSensor(2); break;
        case 2: if (Gpio_ReadPin(GPIO_A, 2) == LOW) Process_FloorSensor(3); break;
        case 3: if (Gpio_ReadPin(GPIO_A, 3) == LOW) Process_FloorSensor(4); break;
        case 4:
            if (Gpio_ReadPin(GPIO_B, 4) == LOW) {
                Elevator_RunFSM(&elev_a, ELEV_EVENT_EMERGENCY_TOGGLE);
            }
            break;
        case 5: if (Gpio_ReadPin(GPIO_C, 5) == LOW) Dispatcher_AddCall(1, 1); break;  /* U1 */
        case 6: if (Gpio_ReadPin(GPIO_C, 6) == LOW) Dispatcher_AddCall(2, 2); break;  /* D2 */
        case 7: if (Gpio_ReadPin(GPIO_C, 7) == LOW) Dispatcher_AddCall(2, 1); break;  /* U2 */
        case 8: if (Gpio_ReadPin(GPIO_B, 8) == LOW) Master_AddCabinRequest(1); break;
        case 9: if (Gpio_ReadPin(GPIO_B, 9) == LOW) Master_AddCabinRequest(2); break;
        case 10: if (Gpio_ReadPin(GPIO_B, 10) == LOW) Master_AddCabinRequest(3); break;
        case 11: if (Gpio_ReadPin(GPIO_B, 11) == LOW) Master_AddCabinRequest(4); break;
        case 12: if (Gpio_ReadPin(GPIO_C, 12) == LOW) Dispatcher_AddCall(3, 2); break; /* D3 */
        case 13: if (Gpio_ReadPin(GPIO_C, 13) == LOW) Dispatcher_AddCall(3, 1); break; /* U3 */
        case 14: if (Gpio_ReadPin(GPIO_C, 14) == LOW) Dispatcher_AddCall(4, 2); break; /* D4 */
        default: break;
    }
}

int main(void) {
    Elevator_InitContext(&elev_a, 1);
    Elevator_InitContext(&elev_b, 1);
    Peripheral_Init();
    lcd_clear();
    lcd_send_string("Master Ready");

    while (1) {
        /* Process EXTI Events */
        for (uint8_t line = 0; line < 15; line++) {
            if (exti_triggered[line]) {
                exti_triggered[line] = 0;
                Process_ExtiLine(line);
            }
        }

        /* UART RX */
        uint8_t rx_data;
        if (Usart1_GetByte(&rx_data) && rx_data == 'R') {
            Elevator_RunFSM(&elev_a, ELEV_EVENT_EMERGENCY_TOGGLE);
        }

        /* Process Timer Events */
        if (door_timeout_flag) {
            door_timeout_flag = 0;
            Elevator_RunFSM(&elev_a, ELEV_EVENT_DOOR_TIMEOUT);
        }

        if (spi_trigger_flag) {
            spi_trigger_flag = 0;

            if (spi_busy) {
                spi_fault_counter++;
                if (spi_fault_counter >= 4) spi_fault_flag = 1;
            } else {
                Enter_Critical();
                SPI_PackFrame(&elev_a, master_tx_packet);
                spi_busy = 1;
                Gpio_WritePin(GPIO_A, 4, LOW);
                Spi1_StartAsync(master_tx_packet, slave_rx_packet, 8, SpiMasterComplete_Callback);
                Exit_Critical();
            }

            Timer_DelayMsAsync(TIMER5, 50, SpiTimer_Callback);
        }

        if (spi_rx_ready) {
            spi_rx_ready = 0;
            if (SPI_ValidateFrame(slave_rx_packet)) {
                spi_fault_counter = 0;
                spi_fault_flag = 0;
                Enter_Critical();
                SPI_UnpackFrame(slave_rx_packet, &elev_b);
                Exit_Critical();
            } else {
                spi_fault_counter++;
                if (spi_fault_counter >= 4) spi_fault_flag = 1;
            }
        }

        if (uart_trigger_flag) {
            uart_trigger_flag = 0;
            char buf[80];
            snprintf(buf, sizeof(buf), "[A] St:%d Fl:%d Tg:%d | [B] St:%d Fl:%d Tg:%d | SPI:%s\r\n",
                     elev_a.state, elev_a.current_floor, elev_a.target_floor,
                     elev_b.state, elev_b.current_floor, elev_b.target_floor,
                     spi_fault_flag ? "FAULT" : "OK");
            Usart1_TransmitString(buf);
            Timer_DelayMsAsync(TIMER2, 500, UartTimer_Callback);
        }

        /* Run Dispatcher and FSM */
        Master_SelectNextCabinTarget();
        Dispatcher_ReevaluateQueue(&elev_a, &elev_b, spi_fault_flag);

        if ((elev_a.state == ELEV_IDLE || elev_a.state == ELEV_DOOR_OPEN) &&
            (elev_a.request_mask & (1 << (elev_a.current_floor - 1)))) {
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
