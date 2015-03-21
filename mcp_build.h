#pragma once

void build_cmd(char **words, MCPacketQueue *sq, MCPacketQueue *cq);
void build_clear();
void build_cancel();
void build_update();
void build_progress(MCPacketQueue *sq, MCPacketQueue *cq);
void brec_blockupdate(MCPacket *pkt);
void brec_blockplace(MCPacket *pkt);
