#include "elevator.h"
#include "../Critical/critical.h"

/* stdlib for abs */
static inline uint8_t get_abs_diff(uint8_t a, uint8_t b) {
    return (a > b) ? (a - b) : (b - a);
}

void Elevator_InitContext(ElevatorContext_t* ctx, uint8_t initial_floor) {
    if(!ctx) return;
    Enter_Critical();
    ctx->state = ELEV_IDLE;
    ctx->current_floor = initial_floor;
    ctx->target_floor = initial_floor;
    ctx->direction = 0; /* NONE */
    ctx->request_mask = 0;
    ctx->emergency_flag = 0;
    ctx->door_open_flag = 0;
    ctx->spi_alive = 1;
    Exit_Critical();
}

void Elevator_UpdateMotor(ElevatorContext_t* ctx) {
    if(!ctx) return;
    
    /* PWM logic based on state */
    switch(ctx->state) {
        case ELEV_IDLE:
        case ELEV_DOOR_OPEN:
        case ELEV_EMERGENCY:
        case ELEV_INDEPENDENT: /* Motor off if independent without internal logic defined, or let's assume it behaves similar for safety. */
            Pwm_SetDutyPercent(TIMER4, PWM_CHANNEL_1, 0);
            break;
            
        case ELEV_MOVING_UP:
        case ELEV_MOVING_DOWN:
        {
            uint8_t diff = get_abs_diff(ctx->target_floor, ctx->current_floor);
            if(diff == 1) {
                Pwm_SetDutyPercent(TIMER4, PWM_CHANNEL_1, 20);
            } else if(diff >= 2) {
                Pwm_SetDutyPercent(TIMER4, PWM_CHANNEL_1, 100);
            } else {
                /* target == current, should be opening door soon */
                Pwm_SetDutyPercent(TIMER4, PWM_CHANNEL_1, 0);
            }
            break;
        }
    }
}
