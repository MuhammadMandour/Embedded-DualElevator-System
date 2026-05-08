#ifndef DISPATCHER_H
#define DISPATCHER_H

#include "../Elevator/elevator.h"

#define MAX_PENDING_CALLS 16

typedef struct {
    uint8_t floor;
    uint8_t direction; /* 1=UP, 2=DOWN */
} CallRequest_t;

/* Add a new call to the queue, dropping oldest if full */
void Dispatcher_AddCall(uint8_t floor, uint8_t direction);

/* Calculate score for a single elevator for a given call */
uint8_t Dispatcher_CalculateScore(ElevatorContext_t* ctx, uint8_t call_floor, uint8_t call_dir, uint8_t spi_fault_flag);

/* Reevaluate the queue and assign pending calls to elev A or elev B */
void Dispatcher_ReevaluateQueue(ElevatorContext_t* elev_a, ElevatorContext_t* elev_b, uint8_t spi_fault_flag);

#endif /* DISPATCHER_H */
