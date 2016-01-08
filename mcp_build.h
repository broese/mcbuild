#pragma once

void build_cmd(char **words, MCPacketQueue *sq, MCPacketQueue *cq);
void build_clear(MCPacketQueue *sq, MCPacketQueue *cq);
void build_cancel(MCPacketQueue *sq, MCPacketQueue *cq);
void build_pause();
void build_update();
void build_progress(MCPacketQueue *sq, MCPacketQueue *cq);
int  build_packet(MCPacket *pkt, MCPacketQueue *sq, MCPacketQueue *cq);
void build_preview_transmit(MCPacketQueue *cq);

void build_sload(const char *name, char *reply);
void build_dump_plan();
void calculate_material(int plan);
int prefetch_material(MCPacketQueue *sq, MCPacketQueue *cq, bid_t mat);
int find_evictable_slot();
