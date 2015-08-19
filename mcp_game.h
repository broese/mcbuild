#pragma once

uint64_t gettimestamp();
void chat_message(const char *str, MCPacketQueue *q, const char *color, int pos);

void gm_packet(MCPacket *pkt, MCPacketQueue *tq, MCPacketQueue *bq);
void gm_reset();
void gm_async(MCPacketQueue *sq, MCPacketQueue *cq);

void gmi_change_held(MCPacketQueue *sq, MCPacketQueue *cq, int sid, int notify_client);
void gmi_swap_slots(MCPacketQueue *sq, MCPacketQueue *cq, int sa, int sb);
void handle_command(char *str, MCPacketQueue *tq, MCPacketQueue *bq);
