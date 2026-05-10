#ifndef ELEVATOR_H
#define ELEVATOR_H

#include "../Lib/Std_Types.h"
#include "../Pwm/Pwm.h"
#include "../Timer/Timer.h"

typedef enum {
    ELEV_IDLE        = 0,
    ELEV_MOVING_UP   = 1,
    ELEV_MOVING_DOWN = 2,
    ELEV_DOOR_OPEN   = 3,
    ELEV_EMERGENCY   = 4,
    ELEV_INDEPENDENT = 5
} ElevatorState_t;

typedef enum {
    ELEV_EVENT_NONE = 0,
    ELEV_EVENT_TARGET_UPDATED,
    ELEV_EVENT_FLOOR_REACHED,
    ELEV_EVENT_DOOR_TIMEOUT,
    ELEV_EVENT_EMERGENCY_TOGGLE,
    ELEV_EVENT_SPI_TIMEOUT
} ElevatorEvent_t;

typedef struct {
    ElevatorState_t state;
    uint8_t current_floor;
    uint8_t target_floor;
    uint8_t direction; /* 0=NONE, 1=UP, 2=DOWN */
    uint8_t request_mask; /* bit0=F1, bit1=F2, bit2=F3, bit3=F4 */
    uint8_t emergency_flag;
    uint8_t door_open_flag;
    uint8_t spi_alive;
} ElevatorContext_t;

void Elevator_InitContext(ElevatorContext_t* ctx, uint8_t initial_floor);
void Elevator_RunFSM(ElevatorContext_t* ctx, ElevatorEvent_t event);
void Elevator_UpdateMotor(ElevatorContext_t* ctx);

#endif /* ELEVATOR_H */