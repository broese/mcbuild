#pragma once

#include "mcp_ids.h"

typedef uint8_t uuid_t[16];
typedef int32_t fpint;

typedef struct {
    int32_t     eid;
    uuid_t      uuid;
    fpint       x,y,z;
    char        yaw,pitch;
    short       item;
    //TODO:     metadata;
} SpawnPlayer;

SpawnPlayer * dec_SpawnPlayer(SpawnPlayer &pkt, uint8_t *p);
uint8_t *     enc_SpawnPlayer(SpawnPlayer &pkt, lh_buf_t *tx);

