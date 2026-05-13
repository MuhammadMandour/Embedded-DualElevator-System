#include "dispatcher.h"
#include "../Critical/critical.h"

/* -----------------------------------------------------------------------
 * Pending-call queue
 * pending_head  : next write slot
 * pending_tail  : next read slot
 * pending_count : number of valid entries
 * ----------------------------------------------------------------------- */
static CallRequest_t pending_calls[MAX_PENDING_CALLS];

/* Definitions live here; other translation units that need them must
 * declare them as  extern volatile uint8_t  in their own .c files
 * (or share them through dispatcher.h).                                   */
volatile uint8_t pending_head  = 0;
volatile uint8_t pending_tail  = 0;
volatile uint8_t pending_count = 0;

/* ----------------------------------------------------------------------- */
static inline uint8_t get_abs_diff_disp(uint8_t a, uint8_t b)
{
    return (a > b) ? (a - b) : (b - a);
}

/* ----------------------------------------------------------------------- */
void Dispatcher_AddCall(uint8_t floor, uint8_t direction)
{
    Enter_Critical();

    pending_calls[pending_head].floor     = floor;
    pending_calls[pending_head].direction = direction;
    pending_head = (pending_head + 1) % MAX_PENDING_CALLS;

    if (pending_count < MAX_PENDING_CALLS) {
        pending_count++;
    } else {
        /* Overflow: oldest call dropped — advance tail to match overwrite */
        pending_tail = (pending_tail + 1) % MAX_PENDING_CALLS;
        /* pending_count stays == MAX_PENDING_CALLS, which is correct       */
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

/* ----------------------------------------------------------------------- */
void Dispatcher_ReevaluateQueue(ElevatorContext_t *elev_a,
                                 ElevatorContext_t *elev_b,
                                 uint8_t            spi_fault_flag)
{
    if (pending_count == 0) return;

    Enter_Critical();

    uint8_t       count      = pending_count;
    CallRequest_t temp_queue[MAX_PENDING_CALLS];
    uint8_t       temp_count = 0;

    for (uint8_t i = 0; i < count; i++) {
        uint8_t       idx  = (pending_tail + i) % MAX_PENDING_CALLS;
        CallRequest_t call = pending_calls[idx];

        uint8_t score_a = Dispatcher_CalculateScore(elev_a, call.floor, call.direction, 0);

        uint8_t score_b = (spi_fault_flag == 1)
                          ? 99u
                          : Dispatcher_CalculateScore(elev_b, call.floor, call.direction, 0);

        uint8_t assigned = 0;

        if (score_a == 99 && score_b == 99) {
            /* Neither elevator can serve this call right now — keep it     */

        } else if (score_a <= score_b) {
            elev_a->request_mask |= ((uint32_t)1 << (call.floor - 1));
            assigned = 1;

        } else {
            elev_b->request_mask |= ((uint32_t)1 << (call.floor - 1));
            assigned = 1;
        }

        if (!assigned) {
            temp_queue[temp_count++] = call;
        }
    }

    /* Rebuild the pending queue from unassigned calls only */
    pending_tail  = 0;
    pending_head  = temp_count;
    pending_count = temp_count;
    for (uint8_t i = 0; i < temp_count; i++) {
        pending_calls[i] = temp_queue[i];
    }

    Exit_Critical();
}