#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>

#include "mcp_arg.h"
#include "mcp_ids.h"
#include "mcp_gamestate.h"

// count the number of format specs in a string
static inline int count_fmt(const char *fmt) {
    int i, c=0;
    for(i=0; fmt[i] && fmt[i+1]; i++)
        if (fmt[i] == '%')
            if (fmt[++i]!='%') c++; // skip %%

    return c;
}

int mcparg_parse(char **words, mcpopt *opt, ...) {
    int i,ui=0;
    int lasterr=0;

    char *value = NULL;

    // locate the option in the words and extract the value
    for(i=0; words[i] && !value; i++) {
        char *eq = index(words[i], '=');
        if (!eq) {
            // this looks like an unnamed option
            if (ui == opt->unidx) {
                // we have matching index
                value = words[i];
                break;
            }
            ui++;
        }
        else {
            // named option
            int j;
            for(j=0; opt->names[j]; j++) {
                int nlen = eq-words[i];
                if (!strncmp(words[i],opt->names[j],nlen)) {
                    // we have a matching name
                    value = eq+1;
                    break;
                }
            }
        }
    }

    if (!value) return MCPARG_NOT_FOUND;

    va_list args;

    // try to parse the value using all offered format options
    for(i=0; opt->forms[i]; i++) {
        int nf = count_fmt(opt->forms[i]);
        va_start (args, opt);
        if (vsscanf(value, opt->forms[i], args) == nf) {
            va_end (args);
            return i;
        }
        va_end (args);
    }

    return MCPARG_NOT_PARSED;
}

int mcparg_find(char **words, ...) {
    va_list args;
    va_start (args, words);
    char * name;
    while(1) {
        name = va_arg(args, char *);
        if (!name) break;
        int i;
        for(i=0; words[i]; i++) {
            if (!strcmp(words[i],name)) {
                va_end (args);
                return 1;
            }
        }
    }
    va_end (args);

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Helpers for other modules

// Building material
bid_t mcparg_parse_material(char **words, char *reply, int pos) {
    // try to parse material specified explicitly
    mcpopt opt_mat = {{"material","mat","m",NULL}, pos,
                      {"0x%x:%d","%d:%d","%3$[^:]:%2$d","%3$[^:]:%4$s","0x%x","%d","%3$s",NULL}};
    int bid=-1, meta=0;
    char sbid[4096], smeta[4096]; sbid[0]=0; smeta[0]=0;

    switch(mcparg_parse(words, &opt_mat, &bid, &meta, sbid, smeta)) {
        case 0:
        case 1:
        case 4:
        case 5:
            break;
        case 2:
        case 6:
            bid = find_bid_name(sbid);
            if (bid<0) sprintf(reply, "Could not find material name %s", sbid);
            break;
        case 3:
            bid = find_bid_name(sbid);
            if (bid<0) { sprintf(reply, "Could not find material name %s", sbid); break; }
            meta = find_meta_name(bid, smeta);
            if (meta<0) sprintf(reply, "Could not find meta name %s", smeta);
            break;
        default: {
            // nothing specified, take the same material the player is currently holding
            slot_t *s = &gs.inv.slots[gs.inv.held+36];
            if (s->item > 0 && !(ITEMS[s->item].flags&I_ITEM)) {
                bid = s->item;
                meta = s->damage;
            }
            else {
                bid = -1;
            }
        }
    }
    if (reply[0]) return BLOCKTYPE(0,0);

    if (bid<0) {
        sprintf(reply, "You must specify material - either explicitly with "
                "mat=<bid>[,<meta>] or by holding a placeable block");
        return BLOCKTYPE(0,0);
    }

    if (ITEMS[bid].flags&I_SLAB) {
        // for slab blocks additionally parse the upper/lower placement
        if (mcparg_find(words,"upper","up","u","high","h",NULL))
            meta |= 8;
        else
            meta &= 7;
    }

    return BLOCKTYPE(bid,meta);
}

////////////////////////////////////////////////////////////////////////////////
// Test function

#if TEST

int main(int ac, char **av) {
    char **words = av+1;

    char reply[4096]; reply[0] = 0;
    char buf[256];

    bid_t mat = mcparg_parse_material(words, reply, 0);

    if (reply[0])
        printf("Error: %s\n", reply);
    else
        printf("Material=%d:%d (%s)\n",mat.bid,mat.meta,get_bid_name(buf, mat));

    return 0;
}

#endif
