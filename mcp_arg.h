#pragma once

#include "mcp_packet.h"

// error codes
#define MCPARG_NOT_FOUND     (-1)   // option not found
#define MCPARG_NOT_PARSED    (-2)   // option found but has wrong format
#define MCPARG_LOOKUP_FAILED (-3)   // option found and has correct format, but the 
                                    // specified ID (e.g. material name) is incorrect

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

// parse next option
#define ARG(func,names,var)                                             \
    switch(argf_##func(&ad, words, names, &var)) {                      \
        case MCPARG_NOT_FOUND:                                          \
            ARG_NOTFOUND = 1;                                           \
            break;                                                      \
        case MCPARG_NOT_PARSED:                                         \
            sprintf(reply, "Failed to parse %s option. Correct format: %s",  #func, argfmt_##func); \
            break;                                                      \
        case MCPARG_LOOKUP_FAILED:                                      \
            sprintf(reply, "Failed to parse %s option. Name lookup failed", #func); \
            break;                                                      \
    }

// require that the option is found, otherwise abort
#define ARGREQ(func,names,var)                          \
    ARG(func,names,var);                                \
    if (reply[0]) goto Error;                           \
    if (ARG_NOTFOUND) {                                 \
        sprintf(reply, "Option %s not found", #func);   \
        goto Error;                                     \
    }

// use a default value for an option if nothing is found
#define ARGDEF(func,names,var,val)                      \
    ARG(func,names,var);                                \
    if (reply[0]) goto Error;                           \
    if (ARG_NOTFOUND) {                                 \
        var = val;                                      \
    }

// special case for the material parsing
#define ARGMAT(names,var,val)                                           \
    ARGDEF(mat,names,var,val);                                          \
    if (var.bid==0 || var.bid==0xfff) {                                 \
        sprintf(reply, "Specify material - either explicitly or by holding a buildable block"); \
        goto Error;                                                     \
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
    int32_t bpsx, bpsz, bpsy;   // buildplan size
} arg_defaults;

////////////////////////////////////////////////////////////////////////////////

int argflag(char **words, char **names);

int argf_size(arg_defaults *ad, char **words, char **names, size3_t *sz);
extern const char *argfmt_size;

int argf_diam(arg_defaults *ad, char **words, char **names, float *diam);
extern const char *argfmt_diam;

int argf_pivot(arg_defaults *ad, char **words, char **names, pivot_t *pivot);
extern const char *argfmt_pivot;

int argf_offset(arg_defaults *ad, char **words, char **names, off3_t *offset);
extern const char *argfmt_offset;

int argf_pos(arg_defaults *ad, char **words, char **names, off3_t *pos);
extern const char *argfmt_pos;

int argf_mat(arg_defaults *ad, char **words, char **names, bid_t *mat);
extern const char *argfmt_mat;

int argf_dir(arg_defaults *ad, char **words, char **names, int *dir);
extern const char *argfmt_dir;

int argf_count(arg_defaults *ad, char **words, char **names, int *count);
const char *argfmt_count;
