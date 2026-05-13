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

void Elevator_RunFSM(ElevatorContext_t* ctx, ElevatorEvent_t event) {
    if(!ctx) return;

    if (event == ELEV_EVENT_EMERGENCY_TOGGLE) {
        if (ctx->state == ELEV_EMERGENCY) {
            ctx->state = ELEV_IDLE;
        } else {
            ctx->state = ELEV_EMERGENCY;
        }
        ctx->direction = 0;
        ctx->emergency_flag = (ctx->state == ELEV_EMERGENCY);
        ctx->door_open_flag = 0;
        return;
    }

    if (event == ELEV_EVENT_SPI_TIMEOUT) {
        ctx->state = ELEV_INDEPENDENT;
        ctx->spi_alive = 0;
        ctx->direction = 0;
        ctx->door_open_flag = 0;
        return;
    }

    if (ctx->state == ELEV_EMERGENCY) return;

    switch (ctx->state) {
        case ELEV_IDLE:
        case ELEV_INDEPENDENT:
        case ELEV_DOOR_OPEN:
            if (event == ELEV_EVENT_FLOOR_REACHED) {
                ctx->state = ELEV_DOOR_OPEN;
                ctx->direction = 0;
                ctx->door_open_flag = 1;
                ctx->request_mask &= ~(1 << (ctx->current_floor - 1));
            } else if (event == ELEV_EVENT_TARGET_UPDATED) {
                if (ctx->current_floor < ctx->target_floor) {
                    ctx->state = ELEV_MOVING_UP;
                    ctx->direction = 1;
                    ctx->door_open_flag = 0;
                } else if (ctx->current_floor > ctx->target_floor) {
                    ctx->state = ELEV_MOVING_DOWN;
                    ctx->direction = 2;
                    ctx->door_open_flag = 0;
                }
            } else if (event == ELEV_EVENT_DOOR_TIMEOUT) {
                ctx->state = ctx->spi_alive ? ELEV_IDLE : ELEV_INDEPENDENT;
                ctx->direction = 0;
                ctx->door_open_flag = 0;
            }
            break;

        case ELEV_MOVING_UP:
        case ELEV_MOVING_DOWN:
            if (event == ELEV_EVENT_FLOOR_REACHED) {
                ctx->state = ELEV_DOOR_OPEN;
                ctx->direction = 0;
                ctx->door_open_flag = 1;
                ctx->request_mask &= ~(1 << (ctx->current_floor - 1));
            }
            break;

        case ELEV_EMERGENCY:
            break;
    }

    ctx->emergency_flag = (ctx->state == ELEV_EMERGENCY);
}

void Elevator_UpdateMotor(ElevatorContext_t* ctx) {
    if(!ctx) return;
    
    /* PWM logic based on state */
    switch(ctx->state) {
        case ELEV_IDLE:
        case ELEV_DOOR_OPEN:
        case ELEV_EMERGENCY:
            Pwm_SetDutyPercent(TIMER4, PWM_CHANNEL_1, 0);
            break;
            
        case ELEV_MOVING_UP:
        case ELEV_MOVING_DOWN:
        case ELEV_INDEPENDENT:
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