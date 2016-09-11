/*
 Authors:
 Copyright 2012-2015 by Eduard Broese <ed.broese@gmx.de>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version
 2 of the License, or (at your option) any later version.
*/

#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

#include "helpers.h"

////////////////////////////////////////////////////////////////////////////////
// Time

uint64_t gettimestamp() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t ts = (uint64_t)tv.tv_sec*1000000+(uint64_t)tv.tv_usec;
    return ts;
}

////////////////////////////////////////////////////////////////////////////////
// Token bucket rate-limiter

tokenbucket * tb_init(tokenbucket *tb, int64_t interval, int64_t burst) {
    if (!tb) tb = (tokenbucket *)malloc(sizeof(tokenbucket));
    assert(tb);

    tb->last = gettimestamp();
    tb->level = burst;
    tb->interval = interval;
    tb->burst = burst;

    return tb;
}

int tb_event(tokenbucket *tb, uint64_t size) {
    uint64_t ts = gettimestamp();
    uint64_t level = tb->level + (ts-tb->last)/tb->interval; // currently available token level
    if (level > tb->burst) level = tb->burst;

    if (size > level) return 0; // disallow this event

    tb->level = level-size;
    tb->last = ts;
    return size;
}

