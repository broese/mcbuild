#pragma once

#include "ids.h"
#include "gamestate.h"

#include "lh_files.h"

void chat_message(const char *str, lh_buf_t *buf, const char *color);
void hole_radar(lh_buf_t *client);
void find_tunnels(lh_buf_t *answer, int l, int h);
