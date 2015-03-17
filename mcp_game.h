#pragma once

uint64_t gettimestamp();
void chat_message(const char *str, MCPacketQueue *q, const char *color, int pos);

void gm_packet(MCPacket *pkt, MCPacketQueue *tq, MCPacketQueue *bq);
void gm_reset();
void gm_async(MCPacketQueue *sq, MCPacketQueue *cq);

void gmi_change_held(MCPacketQueue *sq, MCPacketQueue *cq, int sid, int notify_client);
