#pragma once

void build_cmd(char **words, MCPacketQueue *sq, MCPacketQueue *cq);
void build_clear();
void build_cancel();
void build_update();
void build_progress(MCPacketQueue *sq, MCPacketQueue *cq);
int  build_packet(MCPacket *pkt, MCPacketQueue *sq, MCPacketQueue *cq);

void build_sload(const char *name, char *reply);
void build_dump_plan();
void calculate_material(int plan);
int prefetch_material(MCPacketQueue *sq, MCPacketQueue *cq, bid_t mat);
int find_evictable_slot();
