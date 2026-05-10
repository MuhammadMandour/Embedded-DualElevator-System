#include "dispatcher.h"
#include "../Critical/critical.h"

static CallRequest_t pending_calls[MAX_PENDING_CALLS];
extern volatile uint8_t pending_head;
extern volatile uint8_t pending_tail;
extern volatile uint8_t pending_count;

volatile uint8_t pending_head = 0;
volatile uint8_t pending_tail = 0;
volatile uint8_t pending_count = 0;

extern uint8_t master_hallway_mask_b;

static inline uint8_t get_abs_diff_disp(uint8_t a, uint8_t b) {
    return (a > b) ? (a - b) : (b - a);
}

void Dispatcher_AddCall(uint8_t floor, uint8_t direction) {
    Enter_Critical();
    pending_calls[pending_head].floor = floor;
    pending_calls[pending_head].direction = direction;
    pending_head = (pending_head + 1) % MAX_PENDING_CALLS;
    
    if (pending_count < MAX_PENDING_CALLS) {
        pending_count++;
    } else {
        /* Overflow: oldest call dropped — circular overwrite */
        pending_tail = (pending_tail + 1) % MAX_PENDING_CALLS;
    }
    Exit_Critical();
}

uint8_t Dispatcher_CalculateScore(ElevatorContext_t* ctx, uint8_t call_floor, uint8_t call_dir, uint8_t spi_fault_flag) {
    if(!ctx) return 99;
    
    if (ctx->state == ELEV_EMERGENCY || ctx->state == ELEV_INDEPENDENT) return 99;
    
    if (ctx->state == ELEV_IDLE && ctx->current_floor == call_floor) {
        return 2; /* Immediate */
    }
    
    if (ctx->state == ELEV_MOVING_UP || ctx->state == ELEV_MOVING_DOWN) {
        uint8_t moving_up = (ctx->state == ELEV_MOVING_UP);
        if (moving_up && call_dir == 1 && call_floor > ctx->current_floor) return 3; /* Perfect Match */
        if (!moving_up && call_dir == 2 && call_floor < ctx->current_floor) return 3; /* Perfect Match */
        
        if (moving_up && call_dir == 1 && call_floor <= ctx->current_floor) return 4; /* Passed Match */
        if (!moving_up && call_dir == 2 && call_floor >= ctx->current_floor) return 4; /* Passed Match */
        
        return 99; /* Opposite Direction */
    }
    
    /* IDLE, not at call floor */
    if (ctx->state == ELEV_IDLE || ctx->state == ELEV_DOOR_OPEN) {
        return 6 + get_abs_diff_disp(ctx->current_floor, call_floor);
    }
    
    return 99;
}

void Dispatcher_ReevaluateQueue(ElevatorContext_t* elev_a, ElevatorContext_t* elev_b, uint8_t spi_fault_flag) {
    if (pending_count == 0) return;
    
    Enter_Critical();
    uint8_t count = pending_count;
    /* We will try to assign all calls. If a call is assigned, we remove it from queue.
       Since we might remove from the middle conceptually, we'll rebuild the queue. */
    CallRequest_t temp_queue[MAX_PENDING_CALLS];
    uint8_t temp_count = 0;
    
    for (uint8_t i = 0; i < count; i++) {
        uint8_t idx = (pending_tail + i) % MAX_PENDING_CALLS;
        CallRequest_t call = pending_calls[idx];
        
        uint8_t score_a = Dispatcher_CalculateScore(elev_a, call.floor, call.direction, 0);
        uint8_t score_b = (spi_fault_flag == 1) ? 99 : Dispatcher_CalculateScore(elev_b, call.floor, call.direction, spi_fault_flag);
        
        /* Check if CommFault */
        if (spi_fault_flag == 1) {
            score_b = 99;
            /* Score 1 condition logic implicitly met by assigning to A if A is valid */
        }
        
        uint8_t assigned = 0;
        
        if (score_a == 99 && score_b == 99) {
            /* Neither can take it right now, keep in queue */
        } else if (score_a <= score_b) {
            /* Assign to A (Tie → Elevator A wins deterministically) */
            elev_a->target_floor = call.floor;
            elev_a->request_mask |= (1 << (call.floor - 1));
            assigned = 1;
        } else {
            /* Assign to B */
            master_hallway_mask_b |= (1 << (call.floor - 1));
            elev_b->request_mask |= (1 << (call.floor - 1));
            assigned = 1;
        }
        
        if (!assigned) {
            temp_queue[temp_count++] = call;
        }
    }
    
    /* Rewrite pending queue */
    pending_tail = 0;
    pending_head = temp_count;
    pending_count = temp_count;
    for (uint8_t i = 0; i < temp_count; i++) {
        pending_calls[i] = temp_queue[i];
    }
    
    Exit_Critical();
}
