#pragma once

#include "mcp_packet.h"

// error codes
#define MCPARG_NOT_FOUND  (-1)
#define MCPARG_NOT_PARSED (-2)

#define MCPARG_MAXNAMES 256
#define MCPARG_MAXFORMS 256

typedef struct {
    const char *names[MCPARG_MAXNAMES]; // NULL-terminated list of possible names of the option
    int unidx;                          // unnamed index (-1 if no unnamed form allowed)
    const char *forms[MCPARG_MAXFORMS]; // possible scanf-format strings suitable for the value parsing
} mcpopt;

int mcparg_parse(char **words, mcpopt *opt, ...);
int mcparg_find(char **words, ...);

////////////////////////////////////////////////////////////////////////////////

int mcparg_parse_material(char **words, int argpos, char *reply, bid_t *mat);
