/*
 Authors:
 Copyright 2012-2015 by Eduard Broese <ed.broese@gmx.de>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version
 2 of the License, or (at your option) any later version.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <ctype.h>

#include "mcp_arg.h"
#include "mcp_ids.h"

// count the number of format specs in a string
// this is needed by mcparg_parse to check if the format string
// was correctly matched by sscanf with all arguments
static inline int count_fmt(const char *fmt) {
    int i, c=0;
    for(i=0; fmt[i] && fmt[i+1]; i++)
        if (fmt[i] == '%')
            if (fmt[++i]!='%') c++; // skip %%

    return c;
}


////////////////////////////////////////////////////////////////////////////////

// parse the arguments using the provided options spec
// words : tokenized commandline
// names : possible names for the option
// fmt   : possible format strings for the option
int argparse(char **words, char **names, char **fmt, ...) {
    int i,j;
    char *value = NULL; int ni;   // the pointer (to value) and the index of a named option
    char *unvalue = NULL; int ui; // the pointer and the index of the first unnamed option

    // locate the option in the words and extract the value
    for(i=0; words[i] && !value; i++) {
        if (words[i][0] == '-') continue; // skip flag-like options

        char *eq = index(words[i], '=');
        if (!eq && !unvalue) {
            // this looks like an unnamed option
            unvalue = words[i];
            ui = i;
        }
        else {
            // named option
            for(j=0; names[j]; j++) {
                int nlen = eq-words[i];
                if (!strncmp(words[i],names[j],nlen)) {
                    // we have a matching name
                    value = eq+1;
                    ni = i;
                    break;
                }
            }
        }
    }

    // check if we found the named option and if not if can use an unnamed one
    if (!value) {
        if (unvalue) {
            value = unvalue;
            ni = ui;
        }
        else
            return MCPARG_NOT_FOUND;  // nothing found
    }

    va_list args;

    // try to parse the value using all offered format options
    for(i=0; fmt[i]; i++) {
        int nf = count_fmt(fmt[i]);
        va_start (args, fmt);
        if (vsscanf(value, fmt[i], args) == nf) {
            // scanf could correctly parse all arguments in this spec
            va_end (args);

            // remove the successfully parsed option from the list
            for(j=ni; words[j]; j++)
                words[j] = words[j+1];

            // return the index of the format that matched
            return i;
        }
        va_end (args);
    }

    // none of the format specs matched - likely incorrect formatting
    return MCPARG_NOT_PARSED;
}

// parse the arguments using the provided options spec
// words : tokenized commandline
// names : possible names for the option
// fmt   : possible format strings for the option
int argflag(char **words, char **names) {
    int i,j;

    // locate the option in the words and extract the value
    for(i=0; words[i]; i++) {
        if (words[i][0] == '-') {
            char *name = words[i]+1;
            for(j=0; names[j]; j++) {
                if (!strcmp(names[j], name)) {
                    // remove the successfully parsed option from the list
                    for(; words[i]; i++)
                        words[i] = words[i+1];
                    return 1;
                }
            }
        }
    }
    return 0;
}

////////////////////

int argf_size(arg_defaults *ad, char **words, char **names, size3_t *sz) {
    // default name list
    if (!names) names = WORDLIST("size","sz","s");

    // possible option formats
    char ** fmt_size = WORDLIST("%d,%d,%d",
                                "%d,%d",
                                "%d");

    int fi = argparse(words, names, fmt_size, &sz->x, &sz->z, &sz->y);
    switch (fi) {
        case 0:
            break;
        case 1:
            sz->y=1;
            break;
        case 2:
            sz->y=1;
            sz->z=sz->x;
            break;

        case MCPARG_NOT_FOUND:
        case MCPARG_NOT_PARSED:
            return fi;
        default:
            assert(0);
    }

    return 0;
}

const char *argfmt_size = "size=x[,z[,y]]";

////////////////////

int argf_diam(arg_defaults *ad, char **words, char **names, float *diam) {
    // default name list
    if (!names) names = WORDLIST("diameter","diam","d");

    // possible option formats
    char ** fmt_diam = WORDLIST("%f");

    int fi = argparse(words, names, fmt_diam, diam);
    switch (fi) {
        case 0:
            break;

        case MCPARG_NOT_FOUND:
        case MCPARG_NOT_PARSED:
            return fi;
        default:
            assert(0);
    }

    return 0;
}

const char *argfmt_diam = "diam=d";

////////////////////

int argf_pivot(arg_defaults *ad, char **words, char **names, pivot_t *pivot) {
    // default name list
    if (!names) names = WORDLIST("pivot","pv","p","from","pos","at");

    // possible option formats
    char ** fmt_pivot = WORDLIST("%d,%d,%d,%4$[NEWSnews]",
                                 "%d,%d,%d",
                                 "%d,%d,%4$[NEWSnews]",
                                 "%d,%d",
                                 "%5$d");

    char dir[256];
    int32_t dist;

    int fi = argparse(words, names, fmt_pivot, &pivot->pos.x, &pivot->pos.z, &pivot->pos.y, dir, &dist);
    switch (fi) {
        case 0: // explicitly specified coordinates and direction
            pivot->dir = ltodir(dir[0]);
            break;
        case 1: // only coordinates specified - use player's direction as pivot direction
            pivot->dir = ad->pd;
            break;
        case 2: // x,z coordinate and direction - use player's y coordinate
            pivot->dir = ltodir(dir[0]);
            pivot->pos.y = ad->py;
            break;
        case 3: // x,z coordinate only - use player's y and direction
            pivot->pos.y = ad->py;
            pivot->dir = ad->pd;
            break;
        case 4: // only single number specified - assume it's a distance from player's position
            pivot->pos.y = ad->py;
            pivot->dir = ad->pd;
            switch(pivot->dir) {
                case DIR_NORTH:
                    pivot->pos.x = ad->px;
                    pivot->pos.z = ad->pz-dist;
                    break;
                case DIR_SOUTH:
                    pivot->pos.x = ad->px;
                    pivot->pos.z = ad->pz+dist;
                    break;
                case DIR_EAST:
                    pivot->pos.x = ad->px+dist;
                    pivot->pos.z = ad->pz;
                    break;
                case DIR_WEST:
                    pivot->pos.x = ad->px-dist;
                    pivot->pos.z = ad->pz;
                    break;
            }
            break;

        case MCPARG_NOT_FOUND:
        case MCPARG_NOT_PARSED:
            return fi;
        default:
            assert(0);
    }

    return 0;
}

const char *argfmt_pivot = "pivot=distance or pivot=x,z[,y[,dir]]";

////////////////////

int argf_offset(arg_defaults *ad, char **words, char **names, off3_t *offset) {
    // default name list
    if (!names) names = WORDLIST("offset","off","o");

    // possible option formats
    char ** fmt_offset = WORDLIST("%d,%d,%d",
                                  "%d,%d",
                                  "%4$d%5$[RLFBUDrlfbud]",
                                  "%d",
                                  "%5$[RLFBUDrlfbud]");

    int32_t x,y,z,o;
    char dir[4096]; dir[0] = 0;

    int fi = argparse(words, names, fmt_offset, &x, &z, &y, &o, dir);
    switch (fi) {
        case 0: // explicitly specified offset
            break;
        case 1: // x,z offset - assume y=0
            y = 0;
            break;
        case 2: // explicit offset and direction
            switch(tolower(dir[0])) {
                case 'u': x=0;  z=0;  y=o;  break;
                case 'd': x=0;  z=0;  y=-o; break;
                case 'r': x=o;  z=0;  y=0; break;
                case 'l': x=-o; z=0;  y=0; break;
                case 'f': x=0;  z=-o; y=0; break;
                case 'b': x=0;  z=o;  y=0; break;
            }
            break;
        case 3: // x-only offset - assume z=y=0
            y = 0;
            z = 0;
            break;
        case 4: // direction only - calculate offset from the buildplan size
            switch(tolower(dir[0])) {
                case 'u': x=0;  z=0;  y=ad->bpsy;  break;
                case 'd': x=0;  z=0;  y=-ad->bpsy; break;
                case 'r': x=ad->bpsx;  z=0;  y=0; break;
                case 'l': x=-ad->bpsx; z=0;  y=0; break;
                case 'f': x=0;  z=-ad->bpsz; y=0; break;
                case 'b': x=0;  z=ad->bpsz;  y=0; break;
            }
            break;
        case MCPARG_NOT_FOUND:
        case MCPARG_NOT_PARSED:
            return fi;
        default:
            assert(0);
    }
    offset->x = x;
    offset->y = y;
    offset->z = z;

    return 0;
}

const char *argfmt_offset = "offset=x[,z[,y]] or offset=[n]direction";

////////////////////////////////////////////////////////////////////////////////

int argf_pos(arg_defaults *ad, char **words, char **names, off3_t *pos) {
    // default name list
    if (!names) names = WORDLIST("position","pos","p");

    // possible option formats
    char ** fmt_pos = WORDLIST("%d,%d,%d",
                               "%d,%d",
                               "%4$d");

    int32_t dist;

    int fi = argparse(words, names, fmt_pos, &pos->x, &pos->z, &pos->y, &dist);
    switch (fi) {
        case 0: // explicit x,z,y coordinate
            break;
        case 1: // x,z coordinate - use player's y coordinate
            pos->y = ad->py;
            break;
        case 2: // only single number specified - assume it's a distance from player's position
            pos->y = ad->py;
            switch(ad->pd) {
                case DIR_NORTH:
                    pos->x = ad->px;
                    pos->z = ad->pz-dist;
                    break;
                case DIR_SOUTH:
                    pos->x = ad->px;
                    pos->z = ad->pz+dist;
                    break;
                case DIR_EAST:
                    pos->x = ad->px+dist;
                    pos->z = ad->pz;
                    break;
                case DIR_WEST:
                    pos->x = ad->px-dist;
                    pos->z = ad->pz;
                    break;
            }
            break;

        case MCPARG_NOT_FOUND:
        case MCPARG_NOT_PARSED:
            return fi;
        default:
            assert(0);
    }

    return 0;
}

const char *argfmt_pos = "pos=distance or pos=x,z[,y]";

////////////////////

int argf_mat(arg_defaults *ad, char **words, char **names, bid_t *mat) {
    // default name list
    if (!names) names = WORDLIST("material","mat","m");

    // possible option formats
    char ** fmt_mat = WORDLIST("0x%x:%d%5$[^0-9]",        //  0: expl. hex BID+meta+opt    0x2c:1u
                               "0x%x:%d",                 //  1: expl. hex BID+meta        0x2c:9
                               "%d:%d%5$[^0-9]",          //  2: BID+meta+opt              44:1u
                               "%d:%d",                   //  3: BID+meta                  44:9
                               "0x%x:%5$[^0-9]",          //  4: hex BID                   0x2c:u
                               "0x%x",                    //  5: hex BID                   0x2c
                               "%d%5$[^0-9]",             //  6: BID                       44u
                               "%d",                      //  7: BID                       44
                               "%3$[^:]:%2$d%5$[^0-9]",   //  8: bname+meta+opt            stone_slab:1u
                               "%3$[^:]:%2$d",            //  9: bname+meta                stone_slab:9
                               "%3$[^:]:%4$[^:]:%5$s",    // 10: bname+mname+opt           stone_slab:sandstone:u
                               "%3$[^:]:%4$[^:]",         // 11: bname+mname               stone_slab:sandstone
                               "%3$[^:]::%5$[^0-9:]",     // 12: bname+opt                 stone_slab::u
                               "%3$s");                   // 13: bname                     stone_slab

    // try to locate and parse one of the formats for material spec
    int bid=-1, meta=0;
    char sbid[4096], smeta[4096], sopt[4096];
    sbid[0]=0; smeta[0]=0; sopt[0]=0;

    int fi = argparse(words, names, fmt_mat, &bid, &meta, sbid, smeta, sopt);
    switch (fi) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
            break;
        case 8:
        case 9:
        case 12:
        case 13:
            bid = find_bid_name(sbid);
            if (bid<0) {
                printf("Could not find material name %s\n", sbid);
                return MCPARG_LOOKUP_FAILED;
            }
            break;
        case 10:
        case 11:
            bid = find_bid_name(sbid);
            if (bid<0) {
                printf("Could not find material name %s\n", sbid);
                return MCPARG_LOOKUP_FAILED;
            }
            meta = find_meta_name(bid, smeta);
            if (meta<0) {
                printf("Could not find meta name %s\n", smeta);
                return MCPARG_LOOKUP_FAILED;
            }
            break;
        case MCPARG_NOT_FOUND:
        case MCPARG_NOT_PARSED:
            return fi;
        default:
            assert(0);
    }

    if (sopt[0]) {
        // additional option specified
        if (ITEMS[bid].flags&I_SLAB) {
            if (sopt[0]=='u' || sopt[0]=='h')
                meta |= 8;
            else if (sopt[0]=='l')
                meta &= 7;
        }
        //TODO: add options for other block types
    }

    *mat = BLOCKTYPE(bid, meta);

    char buf[256];

    return 0;
}

const char *argfmt_mat = "mat=material[:meta][upper]";

////////////////////

int argf_dir(arg_defaults *ad, char **words, char **names, int *dir) {
    // default name list
    if (!names) names = WORDLIST("direction","dir","d");

    // possible option formats
    char ** fmt_dir = WORDLIST("%[NEWSnews]");

    char dirs[4096]; dirs[0] = 0;

    int fi = argparse(words, names, fmt_dir, dirs);
    switch (fi) {
        case 0: {
            switch(tolower(dirs[0])) {
                case 'n': *dir=DIR_NORTH; break;
                case 's': *dir=DIR_SOUTH; break;
                case 'w': *dir=DIR_WEST; break;
                case 'e': *dir=DIR_EAST; break;
                default : assert(0);
            }
            break;
        }
        case MCPARG_NOT_FOUND:
        case MCPARG_NOT_PARSED:
            return fi;
    }

    return 0;
}

const char *argfmt_dir = "dir=<n|s|w|e>";

////////////////////

int argf_count(arg_defaults *ad, char **words, char **names, int *count) {
    // default name list
    if (!names) names = WORDLIST("count","cnt","c");

    // possible option formats
    char ** fmt_count = WORDLIST("%d");

    int fi = argparse(words, names, fmt_count, count);
    switch(fi) {
        case 0:
            break;
        case MCPARG_NOT_FOUND:
        case MCPARG_NOT_PARSED:
            return fi;
        default:
            assert(0);
    }
    //printf("Matched format >%s<, count=%d\n", fmt_count[fi], *count);

    return 0;
}

const char *argfmt_count = "count=<n>";

////////////////////

int argf_page(arg_defaults *ad, char **words, char **names, int *page) {
    // default name list
    if (!names) names = WORDLIST("page","pg","p");

    // possible option formats
    char ** fmt_page = WORDLIST("%d");

    int fi = argparse(words, names, fmt_page, page);
    switch(fi) {
        case 0:
            break;
        case MCPARG_NOT_FOUND:
        case MCPARG_NOT_PARSED:
            return fi;
        default:
            assert(0);
    }

    return 0;
}

const char *argfmt_page = "page=<n>";
