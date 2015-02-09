#pragma once

void gm_packet(MCPacket *pkt, MCPacketQueue *tq, MCPacketQueue *bq);
void gm_reset();
void gm_async(MCPacketQueue *sq, MCPacketQueue *cq);
