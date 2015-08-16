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
#define WORDLIST(...) (char*[]) { __VA_ARGS__, NULL }

/*
  The ARG_ macros requre a correctly set scope and certain variables:
  - arg_defaults argdefaults : must be initialized with values
  - char ** words : contains the tokenized commandline
  - var : must be defeined and of correct type required by the argf_XXX functions
  - int ARG_NOTFOUND : must be defined in scope before calling ARG()
  - char *reply must be defined
*/

#define ARGSTART                                                            \
    arg_defaults argdefaults;                                               \
    int ARG_NOTFOUND=0;

#define ARG(func,names,var)                                                 \
    switch(argf_##func(&argdefaults, words, names, &var)) {                 \
        case MCPARG_NOT_FOUND: ARG_NOTFOUND = 1; break;                     \
        case MCPARG_NOT_PARSED:                                             \
            sprintf(reply, "Failed to parse %s option. Correct format: %s", \
                    #func, argfmt_##func);                                  \
            return;                                                         \
    }

#define ARGREQUIRE(func)                                \
    if (ARG_NOTFOUND) {                                 \
        sprintf(reply, "Option %s not found", #func);   \
        return;                                         \
    }

// a struct containing all relevant values from gamestate that may be needed
// as default values in the argument parsing. We are passing these values through
// this struct in order to isolate mcparg module from the gamestate module
typedef struct {
    int32_t px,py,pz;   // player position (in block units)
    int     pd;         // player's look direction
    bid_t   mat;        // material currently held by the player
    bid_t   mat2;       // material in the next slot than what player currently holds
    //TODO: current mask
} arg_defaults;

typedef struct {
    int32_t x,y,z;
} off3_t;               // data type - block offset/coordinate

typedef struct {
    int32_t x,y,z;
} size3_t;              // data type - block size

typedef struct {
    off3_t  pos;
    int     dir;
} pivot_t;              // data type - pivot position and orientation
