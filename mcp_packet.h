#pragma once

#include <stdlib.h>
#include <stdint.h>

#include <lh_arr.h>

// special data types used in the MC packets

typedef uint8_t uuid_t[16];
typedef int32_t fixp;

////////////////////////////////////////////////////////////////////////////////

typedef struct {
    int32_t     threshold;
} SetCompression;

typedef struct {
    double x,y,z;
    float  yaw,pitch;
    char   flags;
} PlayerPositionLook;

////////////////////////////////////////////////////////////////////////////////

typedef struct {
    union {
        int32_t pid;                 // for use as Cx_ Sx_ constants defined in mcp_ids.h
        struct {
            unsigned int type  : 24; // raw packet type from the protocol
            unsigned int mode  : 2;  // protocol mode: 0:Idle, 1:Status, 2:Login, 3:Play
            unsigned int _mode : 2;
            unsigned int cl    : 1;  // whether this packet comes from client
            unsigned int _cl   : 3;
        };
    };

    int32_t   ver;      // decoder will mark this with its version, so in
                        // difficult cases you can differentiate between
                        // changed interpretation

    uint8_t * raw;      // raw packet data
    ssize_t   rawlen;

    // various packet types depending on pid
    union {
        SetCompression     p_SetCompression;
        PlayerPositionLook p_PlayerPositionLook;
    };
} MCPacket;

typedef struct {
    lh_arr_declare(MCPacket *,queue);
} MCPacketQueue;

////////////////////////////////////////////////////////////////////////////////

MCPacket * decode_packet(int is_client, uint8_t *p, ssize_t len);
ssize_t    encode_packet(MCPacket *pkt, uint8_t *buf);
void       dump_packet(MCPacket *pkt);
void       free_packet  (MCPacket *pkt);
void       queue_packet (MCPacket *pkt, MCPacketQueue *q);

////////////////////////////////////////////////////////////////////////////////

