/*
 Authors:
 Copyright 2012-2015 by Eduard Broese <ed.broese@gmx.de>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version
 2 of the License, or (at your option) any later version.
*/

#pragma once

#include <stdint.h>

////////////////////////////////////////////////////////////////////////////////
// Useful macros

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define SGN(x)   ((x)?(((x)>0)?1:-1):0)
#define SQ(x)    ((x)*(x))

////////////////////////////////////////////////////////////////////////////////
// Time

uint64_t gettimestamp();

////////////////////////////////////////////////////////////////////////////////
// Tocken bucket rate-limiter

typedef struct {
    uint64_t    last;       // last timestamp when an event was allowed by rate-limiting
    uint64_t    level;      // current number of tokens
    uint64_t    interval;   // average interval, in us
    uint64_t    burst;      // burst size, in tokens
} tokenbucket;

#define TBDEF(name, i, b)                                                      \
    tokenbucket name = { .last=0, .level=b, .interval=i, .burst=b };

tokenbucket * tb_init(tokenbucket *tb, int64_t interval, int64_t burst);
int tb_event(tokenbucket *tb, uint64_t size);
