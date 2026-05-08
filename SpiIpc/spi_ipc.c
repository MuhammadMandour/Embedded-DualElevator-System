#include "spi_ipc.h"

void SPI_PackFrame(ElevatorContext_t* ctx, uint8_t* frame) {
    if(!ctx || !frame) return;

    if (ctx->state == ELEV_MOVING_UP) {
        ctx->direction = 1;
    } else if (ctx->state == ELEV_MOVING_DOWN) {
        ctx->direction = 2;
    } else {
        ctx->direction = 0;
    }
    ctx->emergency_flag = (ctx->state == ELEV_EMERGENCY);
    ctx->door_open_flag = (ctx->state == ELEV_DOOR_OPEN);
    
    frame[0] = SPI_HEADER_BYTE;
    frame[1] = (uint8_t)ctx->state;
    frame[2] = ctx->current_floor;
    frame[3] = ctx->target_floor;
    frame[4] = ctx->direction;
    frame[5] = ctx->request_mask;
    
    uint8_t flags = 0;
    if(ctx->emergency_flag) flags |= (1 << 0);
    if(ctx->door_open_flag) flags |= (1 << 1);
    if(ctx->spi_alive) flags |= (1 << 2);
    frame[6] = flags;
    
    uint8_t checksum = 0;
    for(uint8_t i = 0; i < 7; i++) {
        checksum ^= frame[i];
    }
    frame[7] = checksum;
}

void SPI_UnpackFrame(const uint8_t* frame, ElevatorContext_t* ctx) {
    if(!ctx || !frame) return;
    
    ctx->state = (ElevatorState_t)frame[1];
    ctx->current_floor = frame[2];
    ctx->target_floor = frame[3];
    ctx->direction = frame[4];
    ctx->request_mask = frame[5];
    
    uint8_t flags = frame[6];
    ctx->emergency_flag = (flags & (1 << 0)) ? 1 : 0;
    ctx->door_open_flag = (flags & (1 << 1)) ? 1 : 0;
    ctx->spi_alive = (flags & (1 << 2)) ? 1 : 0;
}

uint8_t SPI_ValidateFrame(const uint8_t* frame) {
    if(!frame) return 0;
    if(frame[0] != SPI_HEADER_BYTE) return 0;
    
    uint8_t checksum = 0;
    for(uint8_t i = 0; i < 7; i++) {
        checksum ^= frame[i];
    }
    
    return (frame[7] == checksum) ? 1 : 0;
}
