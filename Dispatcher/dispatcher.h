#ifndef DISPATCHER_H
#define DISPATCHER_H

#include <stdint.h>
#include "../Elevator/elevator.h"

/* ----------------------------------------------------------------------- *
 * Dispatcher Bitmaps (replaces queue)
 * ----------------------------------------------------------------------- */
extern volatile uint8_t hall_up_requests;
extern volatile uint8_t hall_down_requests;
extern volatile uint8_t slave_new_calls;

/* ----------------------------------------------------------------------- *
 * Public API
 * ----------------------------------------------------------------------- */

/**
 * @brief  Add a hall call to the dispatcher bitmap.
 * @param  floor      Requested floor (1-based)
 * @param  direction  1 = UP, 2 = DOWN
 */
void Dispatcher_AddCall(uint8_t floor, uint8_t direction);

/**
 * @brief  Assign pending bitmap requests to the best available elevator.
 * @param  elev_a         Pointer to local (master) elevator context
 * @param  elev_b         Pointer to remote (slave) elevator context
 * @param  spi_fault_flag 1 = SPI link to elevator B is faulted, 0 = healthy
 */
void Dispatcher_AssignCalls(ElevatorContext_t *elev_a, ElevatorContext_t *elev_b, uint8_t spi_fault_flag);

/**
 * @brief  Score how well an elevator can serve a call.
 *
 * Score table:
 *   2  – elevator is idle/door-open at the exact requested floor
 *   3  – elevator is moving in the same direction, call floor is ahead
 *   4  – elevator is moving in the same direction, call floor already passed
 *   6+ – elevator is idle/door-open at a different floor (+ distance)
 *   99 – elevator cannot serve the call (wrong direction, fault, etc.)
 *
 * @param  ctx            Elevator context pointer (must not be NULL)
 * @param  call_floor     Floor of the pending call
 * @param  call_dir       Direction of the pending call (1=UP, 2=DOWN)
 * @param  spi_fault_flag Reserved; pass 0 for local elevator
 * @return Score (lower is better; 99 means "cannot serve")
 */
uint8_t Dispatcher_CalculateScore(ElevatorContext_t *ctx,
                                   uint8_t            call_floor,
                                   uint8_t            call_dir,
                                   uint8_t            spi_fault_flag);

#endif /* DISPATCHER_H */