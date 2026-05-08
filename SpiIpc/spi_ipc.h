#ifndef SPI_IPC_H
#define SPI_IPC_H

#include "../Lib/Std_Types.h"
#include "../Elevator/elevator.h"

#define SPI_HEADER_BYTE 0xA5

/*
Byte 0: Header    = 0xA5
Byte 1: FSM state (ElevatorState_t enum value)
Byte 2: Current floor (1–4)
Byte 3: Target floor  (1–4)
Byte 4: Direction     (0=NONE, 1=UP, 2=DOWN)
Byte 5: Request bitmask (bit0=F1, bit1=F2, bit2=F3, bit3=F4)
Byte 6: Flags (bit0=Emergency, bit1=DoorOpen, bit2=SPI_Alive)
Byte 7: Checksum = XOR of bytes 0–6
*/

/* Pack an elevator context into an 8-byte SPI frame */
void SPI_PackFrame(ElevatorContext_t* ctx, uint8_t* frame);

/* Unpack an 8-byte SPI frame into an elevator context */
void SPI_UnpackFrame(const uint8_t* frame, ElevatorContext_t* ctx);

/* Validate the checksum of a received frame. Returns 1 if valid, 0 if invalid */
uint8_t SPI_ValidateFrame(const uint8_t* frame);

#endif /* SPI_IPC_H */
