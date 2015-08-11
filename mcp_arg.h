#pragma once

#include "mcp_packet.h"

// error codes
#define MCPARG_NOT_FOUND  (-1)
#define MCPARG_NOT_PARSED (-2)

#define MCPARG_MAXNAMES 256
#define MCPARG_MAXNLEN  256
#define MCPARG_MAXFORMS 256

typedef struct {
    char names[MCPARG_MAXNAMES][MCPARG_MAXNLEN];
    // NULL-terminated list of possible names of the option

    int unidx;
    // unnamed index (-1 if no unnamed form allowed)

    const char *forms[MCPARG_MAXFORMS];
    // possible scanf-format strings suitable for the value parsing
} mcpopt;

int mcparg_parse(char **words, mcpopt *opt, ...);
int mcparg_find(char **words, ...);

////////////////////////////////////////////////////////////////////////////////

int mcparg_parse_material(char **words, int argpos, char *reply, bid_t *mat, const char *suffix);
int mcparg_parse_offset(char **words, int argpos, char *reply, boff_t *off);
int mcparg_parse_direction(char **words, int argpos, char *reply, int *dir);
int mcparg_parse_size(char **words, int argpos, char *reply, int *sx, int *sz, int *sy);

////////////////////////////////////////////////////////////////////////////////

// macro to produce a NULL-terminated char ** - compatible list of words
//FIXME - possible to use a "const char **" everywhere?
#define WORDLIST(...) (const char*[]) { __VA_ARGS__, NULL }
