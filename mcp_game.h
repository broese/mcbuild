#pragma once

static void chat_message(const char *str, MCPacketQueue *q, const char *color, int pos);

void gm_packet(MCPacket *pkt, MCPacketQueue *tq, MCPacketQueue *bq);
void gm_reset();
void gm_async(MCPacketQueue *sq, MCPacketQueue *cq);
