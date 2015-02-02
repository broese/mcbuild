#pragma once

#include <stdlib.h>
#include <stdint.h>

#include <lh_arr.h>

// special data types used in the MC packets

typedef uint8_t uuid_t[16];
typedef int32_t fixp;

////////////////////////////////////////////////////////////////////////////////
// Generic unsupported packet type
// Packets will be decoded as this if there's no support for this type
// The encoder will still be able to correctly encode it back to network format

typedef struct {
    ssize_t     length;
    uint8_t     *data;
} UnknownPacket;

////////////////////////////////////////////////////////////////////////////////

typedef struct {
    int32_t     type;       // packet type, from original MC packet+is_client+STATE_PLAY
                            // therefore - one of the CP_ or SP_ constants
    int32_t     protocol;   // decoder will mark this with its version, so in
                            // difficult cases you can differentiate between 
                            // changed interpretation
    union {
        UnknownPacket p_UnknownPacket;
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

/*
  An idea how to implement support for multiple protocol versions:
  The decode_packet and encode_packet routines always support packets of the
  newest protocol version. The switch-case construct within this function
  cherry-picks the packet types that have changed in the current version,
  and in the default statement it simply calls the function from the previous
  version, that is now renamed to decode_packet_<oldversion>.

  But we need some caution - we need to first determine if the packet is
  supported, according to our supported types table. If it's not supported,
  we return an UnknownPacket, if it's supported, we go into switch-case
  and if there's a block for this type, we decode it according to the latest
  protocol version. If there's no block, we will land in the default statement
  and pass it to the previous version - this is a way of saying - yes, we
  support this packet type, but it's decoded by the previous version as well.

  When a new version comes out, you check for every supported packet type,
  whether the format is still the same. If so, you can simply mark it as 
  supported and it will be automatically decoded by the previous version decoder.
  If it has changed, you may choose to implement it properly or simply mark
  as unsupported.
*/
