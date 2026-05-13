#include "dispatcher.h"
#include "../Critical/critical.h"

/* ----------------------------------------------------------------------- */
volatile uint8_t hall_up_requests = 0;
volatile uint8_t hall_down_requests = 0;
volatile uint8_t slave_new_calls = 0;

static inline uint8_t get_abs_diff_disp(uint8_t a, uint8_t b)
{
    return (a > b) ? (a - b) : (b - a);
}

/* ----------------------------------------------------------------------- */
void Dispatcher_AddCall(uint8_t floor, uint8_t direction)
{
    Enter_Critical();
    if (direction == 1) {
        hall_up_requests |= (1 << (floor - 1));
    } else {
        hall_down_requests |= (1 << (floor - 1));
    }
    Exit_Critical();
}

/* ----------------------------------------------------------------------- */
void Dispatcher_AssignCalls(ElevatorContext_t *elev_a, ElevatorContext_t *elev_b, uint8_t spi_fault_flag)
{
    Enter_Critical();

    for (uint8_t floor = 1; floor <= 4; floor++) {
        if (hall_up_requests & (1 << (floor - 1))) {
            uint8_t score_a = Dispatcher_CalculateScore(elev_a, floor, 1, 0);
            uint8_t score_b = (spi_fault_flag == 1) ? 99u : Dispatcher_CalculateScore(elev_b, floor, 1, 0);

            if (score_a < 99 || score_b < 99) {
                if (score_a <= score_b) {
                    elev_a->request_mask |= (1 << (floor - 1));
                } else {
                    elev_b->request_mask |= (1 << (floor - 1));
                    slave_new_calls |= (1 << (floor - 1));
                }
                hall_up_requests &= ~(1 << (floor - 1));
            }
        }
    }

    for (uint8_t floor = 1; floor <= 4; floor++) {
        if (hall_down_requests & (1 << (floor - 1))) {
            uint8_t score_a = Dispatcher_CalculateScore(elev_a, floor, 2, 0);
            uint8_t score_b = (spi_fault_flag == 1) ? 99u : Dispatcher_CalculateScore(elev_b, floor, 2, 0);

            if (score_a < 99 || score_b < 99) {
                if (score_a <= score_b) {
                    elev_a->request_mask |= (1 << (floor - 1 + 4));
                } else {
                    elev_b->request_mask |= (1 << (floor - 1 + 4));
                    slave_new_calls |= (1 << (floor - 1 + 4));
                }
                hall_down_requests &= ~(1 << (floor - 1));
            }
        }
    }

    Exit_Critical();
}

/* ----------------------------------------------------------------------- */
uint8_t Dispatcher_CalculateScore(ElevatorContext_t *ctx,
                                   uint8_t            call_floor,
                                   uint8_t            call_dir,
                                   uint8_t            spi_fault_flag)
{
    (void)spi_fault_flag;   /* reserved for future per-elevator fault info  */

    if (!ctx) return 99;

    /* Unavailable states */
    if (ctx->state == ELEV_EMERGENCY || ctx->state == ELEV_INDEPENDENT)
        return 99;

    /* Already at the requested floor and idle / door open → best score    */
    if ((ctx->state == ELEV_IDLE || ctx->state == ELEV_DOOR_OPEN) &&
        ctx->current_floor == call_floor)
        return 2;   /* Immediate */

    /* Moving elevator — score only when the call is on the current path   */
    if (ctx->state == ELEV_MOVING_UP || ctx->state == ELEV_MOVING_DOWN) {
        uint8_t moving_up = (ctx->state == ELEV_MOVING_UP);

        /* Call is ahead in the same direction */
        if (moving_up  && call_dir == 1 && call_floor >  ctx->current_floor) return 3;
        if (!moving_up && call_dir == 2 && call_floor <  ctx->current_floor) return 3;

        /* Call was already passed but same direction — needs a round-trip  */
        if (moving_up  && call_dir == 1 && call_floor <= ctx->current_floor) return 4;
        if (!moving_up && call_dir == 2 && call_floor >= ctx->current_floor) return 4;

        /* Opposite direction — cannot serve efficiently */
        return 99;
    }

    /* IDLE or DOOR_OPEN at a different floor: cost = base + distance      */
    if (ctx->state == ELEV_IDLE || ctx->state == ELEV_DOOR_OPEN) {
        return (uint8_t)(6 + get_abs_diff_disp(ctx->current_floor, call_floor));
    }

    return 99;
}
