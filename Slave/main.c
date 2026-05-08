/* === REGISTER MAP ===
   RCC:    0x40023800 | GPIOA: 0x40020000 | GPIOB: 0x40020400
   GPIOC:  0x40020800 | GPIOD: 0x40020C00 | GPIOE: 0x40021000
   EXTI:   0x40013C00 | SYSCFG: 0x40013800
   TIM2:   0x40000000 | TIM3:  0x40000400
   TIM4:   0x40000800 | TIM5:  0x40000C00
   SPI1:   0x40013000 | USART1: 0x40011000
   NVIC:   0xE000E100

   === TIMER ALLOCATION ===
   TIM2 → UART telemetry 500ms        (self-rearming, non-reentrant)
   TIM3 → Door-open 3000ms one-shot   (exclusive — never reused)
   TIM4 → Motor PWM 10kHz             (exclusive — never passed to Timer_DelayMsAsync)
   TIM5 → SPI exchange/watchdog 50ms  (self-rearming, non-reentrant)

   === PWM MATH (10kHz @ 16MHz HSI) ===
   PSC=15, ARR=99 → F = 16MHz / (16 × 100) = 10kHz
   Duty 0% → CCR=0 | Duty 20% → CCR=20 | Duty 100% → CCR=100

   === MOTOR PIN ===
   PB6 / TIM4_CH1 / AF2 on BOTH boards. PA6 = SPI1 MISO only — never PWM.

   === SPI FRAME ===
   [0xA5][State][CurFloor][TgtFloor][Dir][ReqMask][Flags][XOR 0-6]

   === SPI FAULT POLICY ===
   4 consecutive invalid frames → fault flag = 1.
   One valid frame → immediate full reset of counter and flag.

   === HALLWAY MAP (Master PC0–PC5) ===
   PC0=Up@F1 PC1=Dn@F2 PC2=Up@F2 PC3=Dn@F3 PC4=Up@F3 PC5=Dn@F4

   === DISPATCHER SCORES ===
   1=CommFault 2=Immediate 3=PerfectMatch 4=PassedMatch 99=OppDir 6+=IdleNearest
   Tie → Elevator A wins (deterministic by design)

   === VOLATILE RULE ===
   Only scalar flags/counters/indices are volatile.
   Buffers and structs use Enter_Critical/Exit_Critical only.

   === FLOOR SENSOR DEBOUNCE ===
   20ms per-floor lockout via timestamp. Prevents double-trigger on bounce.

   === OVERFLOW POLICY ===
   pending_calls[16]: drop oldest on overflow — circular overwrite.
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
#include "../LCD/lcd.h"

volatile uint8_t spi_fault_flag = 0, spi_fault_counter = 0, spi_watchdog_counter = 0;
volatile uint8_t emergency_flag = 0, door_open_flag = 0, spi_rx_ready = 0;
volatile uint8_t floor_sensor_flags = 0, cabin_button_flags = 0;
volatile ElevatorState_t elev_b_state = ELEV_IDLE;

ElevatorContext_t elev_b;
uint8_t slave_tx_packet[8];
uint8_t slave_rx_packet[8];

static volatile uint32_t global_ms = 0;

static inline uint32_t Get_TickMs(void) {
    uint32_t cnt = *((volatile uint32_t*)(0x40000000 + 0x24));
    return global_ms + cnt;
}

void Door_TimeoutCallback(void) {
    Enter_Critical();
    if (elev_b.state == ELEV_DOOR_OPEN) {
        elev_b.state = ELEV_IDLE;
        elev_b_state = ELEV_IDLE;
    }
    Exit_Critical();
    Elevator_UpdateMotor(&elev_b);
}

void Process_FloorSensor(uint8_t floor) {
    static volatile uint32_t last_trigger_ms[4] = {0, 0, 0, 0};
    uint32_t now = Get_TickMs();
    if ((now - last_trigger_ms[floor - 1]) < 20) return;
    last_trigger_ms[floor - 1] = now;

    Enter_Critical();
    elev_b.current_floor = floor;
    if (elev_b.state == ELEV_MOVING_UP || elev_b.state == ELEV_MOVING_DOWN) {
        if (floor == elev_b.target_floor) {
            elev_b.state = ELEV_DOOR_OPEN;
            elev_b_state = ELEV_DOOR_OPEN;
            elev_b.request_mask &= ~(1 << (floor - 1));
            
            Timer_Stop(TIMER3);
            Timer_DelayMsAsync(TIMER3, 3000, Door_TimeoutCallback);
        }
    }
    Exit_Critical();
    Elevator_UpdateMotor(&elev_b);
}

void EXTI0_Callback(void) {
    if (Gpio_ReadPin(GPIO_A, 0) == LOW) Process_FloorSensor(1);
    if (Gpio_ReadPin(GPIO_B, 0) == LOW) {
        Enter_Critical();
        elev_b.request_mask |= (1 << 0);
        if(elev_b.state == ELEV_IDLE) {
            elev_b.target_floor = 1;
            if(elev_b.target_floor > elev_b.current_floor) elev_b.state = ELEV_MOVING_UP;
            else if(elev_b.target_floor < elev_b.current_floor) elev_b.state = ELEV_MOVING_DOWN;
        }
        Exit_Critical();
        Elevator_UpdateMotor(&elev_b);
    }
}

void EXTI1_Callback(void) {
    if (Gpio_ReadPin(GPIO_A, 1) == LOW) Process_FloorSensor(2);
    if (Gpio_ReadPin(GPIO_B, 1) == LOW) {
        Enter_Critical();
        elev_b.request_mask |= (1 << 1);
        if(elev_b.state == ELEV_IDLE) {
            elev_b.target_floor = 2;
            if(elev_b.target_floor > elev_b.current_floor) elev_b.state = ELEV_MOVING_UP;
            else if(elev_b.target_floor < elev_b.current_floor) elev_b.state = ELEV_MOVING_DOWN;
        }
        Exit_Critical();
        Elevator_UpdateMotor(&elev_b);
    }
}

void EXTI2_Callback(void) {
    if (Gpio_ReadPin(GPIO_A, 2) == LOW) Process_FloorSensor(3);
    if (Gpio_ReadPin(GPIO_B, 2) == LOW) {
        Enter_Critical();
        elev_b.request_mask |= (1 << 2);
        if(elev_b.state == ELEV_IDLE) {
            elev_b.target_floor = 3;
            if(elev_b.target_floor > elev_b.current_floor) elev_b.state = ELEV_MOVING_UP;
            else if(elev_b.target_floor < elev_b.current_floor) elev_b.state = ELEV_MOVING_DOWN;
        }
        Exit_Critical();
        Elevator_UpdateMotor(&elev_b);
    }
}

void EXTI3_Callback(void) {
    if (Gpio_ReadPin(GPIO_A, 3) == LOW) Process_FloorSensor(4);
    if (Gpio_ReadPin(GPIO_B, 3) == LOW) {
        Enter_Critical();
        elev_b.request_mask |= (1 << 3);
        if(elev_b.state == ELEV_IDLE) {
            elev_b.target_floor = 4;
            if(elev_b.target_floor > elev_b.current_floor) elev_b.state = ELEV_MOVING_UP;
            else if(elev_b.target_floor < elev_b.current_floor) elev_b.state = ELEV_MOVING_DOWN;
        }
        Exit_Critical();
        Elevator_UpdateMotor(&elev_b);
    }
}

void EXTI4_Callback(void) {
    /* PA4 CS or PB4 Emergency */
    if (Gpio_ReadPin(GPIO_A, 4) == LOW) {
        /* SPI_CS_ISR */
        for (uint8_t i = 0; i < 8; i++) {
            Spi1_TransmitReceiveByte(slave_tx_packet[i], &slave_rx_packet[i]);
        }
        if (SPI_ValidateFrame(slave_rx_packet)) {
            spi_watchdog_counter = 0;
            Enter_Critical();
            /* extract target/direction */
            if (slave_rx_packet[3] != 0 && elev_b.state == ELEV_IDLE) {
                elev_b.target_floor = slave_rx_packet[3];
                if(elev_b.target_floor > elev_b.current_floor) elev_b.state = ELEV_MOVING_UP;
                else if(elev_b.target_floor < elev_b.current_floor) elev_b.state = ELEV_MOVING_DOWN;
                else elev_b.state = ELEV_DOOR_OPEN;
            }
            Exit_Critical();
            spi_rx_ready = 1;
        }
    }
    
    if (Gpio_ReadPin(GPIO_B, 4) == LOW) {
        Enter_Critical();
        if (elev_b.state == ELEV_EMERGENCY) {
            elev_b.state = ELEV_IDLE;
            elev_b_state = ELEV_IDLE;
        } else {
            elev_b.state = ELEV_EMERGENCY;
            elev_b_state = ELEV_EMERGENCY;
        }
        Exit_Critical();
        Elevator_UpdateMotor(&elev_b);
    }
}

void SPI_WatchdogTick(void) {
    spi_watchdog_counter++;
    if (spi_watchdog_counter >= 4) {
        Enter_Critical();
        if(elev_b.state == ELEV_IDLE || elev_b.state == ELEV_MOVING_UP || elev_b.state == ELEV_MOVING_DOWN) {
            elev_b.state = ELEV_INDEPENDENT;
            elev_b_state = ELEV_INDEPENDENT;
        }
        Exit_Critical();
        Elevator_UpdateMotor(&elev_b);
    }
    Timer_DelayMsAsync(TIMER5, 50, SPI_WatchdogTick);
}

void UART_SendStatus(void) {
    global_ms += 500;
    /* Optional: Slave could also send status, but master handles it in this spec.
       To prevent compilation warnings, we keep it empty or log minimally */
    Timer_DelayMsAsync(TIMER2, 500, UART_SendStatus);
}

void Peripheral_Init(void) {
    Rcc_Init();
    Rcc_Enable(RCC_GPIOA);
    Rcc_Enable(RCC_GPIOB);
    Rcc_Enable(RCC_GPIOE);

    Rcc_Enable(RCC_TIM2);
    Rcc_Enable(RCC_TIM3);
    Rcc_Enable(RCC_TIM4);
    Rcc_Enable(RCC_TIM5);

    Rcc_Enable(RCC_SPI1);
    Rcc_Enable(RCC_USART1);

    /* GPIO Config PA */
    for(uint8_t i=0; i<=3; i++) Gpio_Init(GPIO_A, i, GPIO_INPUT, GPIO_PULL_UP); /* Floor sensors */
    Gpio_Init(GPIO_A, 4, GPIO_INPUT, GPIO_PULL_UP); /* CS input with EXTI */
    Gpio_Init(GPIO_A, 5, GPIO_AF, GPIO_PUSH_PULL); Gpio_SetAF(GPIO_A, 5, GPIO_AF5); /* SCK */
    Gpio_Init(GPIO_A, 6, GPIO_AF, GPIO_PUSH_PULL); Gpio_SetAF(GPIO_A, 6, GPIO_AF5); /* MISO */
    Gpio_Init(GPIO_A, 7, GPIO_AF, GPIO_PUSH_PULL); Gpio_SetAF(GPIO_A, 7, GPIO_AF5); /* MOSI */
    Gpio_Init(GPIO_A, 9, GPIO_AF, GPIO_PUSH_PULL); Gpio_SetAF(GPIO_A, 9, GPIO_AF7); /* TX */

    /* GPIO Config PB */
    for(uint8_t i=0; i<=4; i++) Gpio_Init(GPIO_B, i, GPIO_INPUT, GPIO_PULL_UP); /* Cabin + Emerg */
    Gpio_Init(GPIO_B, 6, GPIO_AF, GPIO_PUSH_PULL); Gpio_SetAF(GPIO_B, 6, GPIO_AF2); /* PWM */

    /* GPIO Config PE */
    for(uint8_t i=7; i<=12; i++) Gpio_Init('E', i, GPIO_OUTPUT, GPIO_PUSH_PULL); /* LCD */

    /* PWM */
    Pwm_Init(TIMER4, PWM_CHANNEL_1, 15, 99);
    Pwm_Start(TIMER4, PWM_CHANNEL_1);

    /* SPI & UART */
    Spi1_Init(SPI_SLAVE, SPI_IDLE_LOW, SPI_SAMPLE_FIRST_TRANSITION);
    Usart1_Init();
    lcd_init();

    /* EXTI */
    Exti_Init(EXTI_LINE_0, EXTI_PORT_A, EXTI_EDGE_FALLING, EXTI0_Callback);
    Exti_Init(EXTI_LINE_0, EXTI_PORT_B, EXTI_EDGE_FALLING, EXTI0_Callback);
    
    Exti_Init(EXTI_LINE_1, EXTI_PORT_A, EXTI_EDGE_FALLING, EXTI1_Callback);
    Exti_Init(EXTI_LINE_1, EXTI_PORT_B, EXTI_EDGE_FALLING, EXTI1_Callback);

    Exti_Init(EXTI_LINE_2, EXTI_PORT_A, EXTI_EDGE_FALLING, EXTI2_Callback);
    Exti_Init(EXTI_LINE_2, EXTI_PORT_B, EXTI_EDGE_FALLING, EXTI2_Callback);

    Exti_Init(EXTI_LINE_3, EXTI_PORT_A, EXTI_EDGE_FALLING, EXTI3_Callback);
    Exti_Init(EXTI_LINE_3, EXTI_PORT_B, EXTI_EDGE_FALLING, EXTI3_Callback);

    Exti_Init(EXTI_LINE_4, EXTI_PORT_A, EXTI_EDGE_FALLING, EXTI4_Callback); /* CS */
    Exti_Init(EXTI_LINE_4, EXTI_PORT_B, EXTI_EDGE_FALLING, EXTI4_Callback); /* Emergency */

    /* Enable EXTI Lines */
    for(uint8_t i=0; i<=4; i++) Exti_Enable(i);

    /* NVIC */
    Nvic_EnableIrq(10); /* EXTI4 (Emergency priority 0,0 conceptual) */
    Nvic_EnableIrq(6);  /* EXTI0 */
    Nvic_EnableIrq(7);  /* EXTI1 */
    Nvic_EnableIrq(8);  /* EXTI2 */
    Nvic_EnableIrq(9);  /* EXTI3 */

    /* Timers */
    Timer_Init(TIMER2, 15999, 500);
    Timer_Init(TIMER3, 15999, 3000);
    Timer_Init(TIMER5, 15999, 50);

    Timer_DelayMsAsync(TIMER2, 500, UART_SendStatus);
    Timer_DelayMsAsync(TIMER5, 50, SPI_WatchdogTick);
}

int main(void) {
    Elevator_InitContext(&elev_b, 1);
    SPI_PackFrame(&elev_b, slave_tx_packet);

    Peripheral_Init();

    lcd_clear();
    lcd_send_string("Slave Ready");

    while (1) {
        if (spi_rx_ready) {
            spi_rx_ready = 0;
            Enter_Critical();
            SPI_PackFrame(&elev_b, slave_tx_packet);
            Exit_Critical();
            Elevator_UpdateMotor(&elev_b);
        }
        
        uint8_t rx_data;
        if (Usart1_GetByte(&rx_data)) {
            if (rx_data == 'R') {
                Enter_Critical();
                if (elev_b.state == ELEV_EMERGENCY) {
                    elev_b.state = ELEV_IDLE;
                    elev_b_state = ELEV_IDLE;
                }
                Exit_Critical();
                Elevator_UpdateMotor(&elev_b);
            }
        }
    }
    return 0;
}
