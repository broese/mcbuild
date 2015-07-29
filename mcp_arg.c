#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>

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

// parse the arguments using the provided options spec
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
            for(j=0; opt->names[j][0]; j++) {
                int nlen = eq-words[i];
                if (!strncmp(words[i],opt->names[j],nlen)) {
                    // we have a matching name
                    value = eq+1;
                    break;
                }
            }
        }
    }

    // named option was not found and no unnamed option with this index located
    if (!value) return MCPARG_NOT_FOUND;

    va_list args;

    // try to parse the value using all offered format options
    for(i=0; opt->forms[i]; i++) {
        int nf = count_fmt(opt->forms[i]);
        va_start (args, opt);
        if (vsscanf(value, opt->forms[i], args) == nf) {
            // scanf could correctly parse all arguments in this spec
            va_end (args);
            return i;
        }
        va_end (args);
    }

    // none of the format specs matched - likely incorrect formatting
    return MCPARG_NOT_PARSED;
}

// find a single flag-like option
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
int mcparg_parse_material(char **words, int argpos, char *reply, bid_t *mat, const char *suffix) {
    // try to parse material specified explicitly
    mcpopt opt_mat = {{"material","mat","m"}, argpos,
                      {  "0x%x:%d%5$[^0-9]",        //  0: expl. hex BID+meta+opt    0x2c:1u
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
                         "%3$s",                    // 13: bname                     stone_slab
                         NULL}};
    // add suffix to the option names if the user specified it
    if (suffix) {
        int i;
        char buf[256];
        for (i=0; opt_mat.names[i][0]; i++) {
            //printf("%s : %zd\n", opt_mat.names[i], strlen(opt_mat.names[i]));
            sprintf(opt_mat.names[i]+strlen(opt_mat.names[i]), "%s", suffix);
        }
    }

    // try to locate and parse one of the formats for material spec
    int bid=-1, meta=0;
    char sbid[4096], smeta[4096], sopt[4096];
    sbid[0]=0; smeta[0]=0; sopt[0]=0;

    switch(mcparg_parse(words, &opt_mat, &bid, &meta, sbid, smeta, sopt)) {
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
            if (bid<0) sprintf(reply, "Could not find material name %s", sbid);
            break;
        case 10:
        case 11:
            bid = find_bid_name(sbid);
            if (bid<0) { sprintf(reply, "Could not find material name %s", sbid); break; }
            meta = find_meta_name(bid, smeta);
            if (meta<0) sprintf(reply, "Could not find meta name %s", smeta);
            break;
        case MCPARG_NOT_FOUND:
            *mat = BLOCKTYPE(0,0);
            return 0;
        case MCPARG_NOT_PARSED:
            sprintf(reply, "Could not parse material specification");
            break;
    }

    if (reply[0]) {
        // material spec was found but parsing failed for any reason
        *mat = BLOCKTYPE(0,0);
        return 0;
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

    *mat = BLOCKTYPE(bid,meta);
    return 1;
}

// Block offset
int mcparg_parse_offset(char **words, int argpos, char *reply, boff_t *off) {
    mcpopt opt_off = {{"offset","off","o"}, argpos,
                      {  "%d,%d,%d",            // 0, x,z,y
                         "%d,%d",               // 1, x,z
                         "%d%4$[rlfbud]",       // 2, offset + direction
                         "%d",                  // 3, x
                         "%4$[rlfbud]",         // 4, direction
                         NULL}};

    // try to locate and parse one of the formats for material spec
    int32_t x,y,z;
    char dir[4096]; dir[0] = 0;

    int match = mcparg_parse(words, &opt_off, &x, &z, &y, dir);
    switch(match) {
        case 0:
            break;
        case 1:
            y=0;
            break;
        case 2:
            switch (tolower(dir[0])) {
                case 'u': y=x;  x=0; z=0; break;
                case 'd': y=-x; x=0; z=0; break;
                case 'r': x=x;  z=0; y=0; break;
                case 'l': x=-x; z=0; y=0; break;
                case 'f': z=-x; x=0; y=0; break;
                case 'b': z=x;  x=0; y=0; break;
                default:
                    sprintf(reply, "unrecognized direction %s", dir);
                    return 0;
            }
            break;
        case 3:
            z=y=0;
            break;
        case 4:
            switch (tolower(dir[0])) {
                case 'u': x=0; z=0; y=off->dy; break;
                case 'd': x=0; z=0; y=-off->dy; break;
                case 'r': x=off->dx; z=0; y=0; break;
                case 'l': x=-off->dx; z=0; y=0; break;
                case 'f': x=0; z=-off->dz; y=0; break;
                case 'b': x=0; z=off->dz; y=0; break;
                default:
                    sprintf(reply, "unrecognized direction %s", dir);
                    return 0;
            }
            break;
        case MCPARG_NOT_FOUND:
            return 0;
        case MCPARG_NOT_PARSED:
            sprintf(reply, "could not parse offset specification");
            return 0;
    }

    off->dx = x;
    off->dy = y;
    off->dz = z;
    return 1;
}

// Direction
int mcparg_parse_direction(char **words, int argpos, char *reply, int *dir) {
    mcpopt opt_dir = {{"direction","dir","d"}, argpos,
                      { "%s",                   // name
                        NULL }};

    char direction[256];
    int match = mcparg_parse(words, &opt_dir, direction);
    switch (match) {
        case 0: {
            switch(direction[0]) {
                case 's':
                case '2':
                    *dir = DIR_SOUTH;
                    break;
                case 'n':
                case '3':
                    *dir = DIR_NORTH;
                    break;
                case 'e':
                case '4':
                    *dir = DIR_EAST;
                    break;
                case 'w':
                case '5':
                    *dir = DIR_WEST;
                    break;
                default:
                    sprintf(reply, "Usage: dir=<south|north|east|west>");
                    return 0;
            }
            break;
        }
        case MCPARG_NOT_PARSED:
            sprintf(reply, "Usage: dir=<south|north|east|west>");
            return 0;
        case MCPARG_NOT_FOUND:
            if (*dir != DIR_SOUTH && *dir != DIR_NORTH &&
                *dir != DIR_EAST && *dir != DIR_WEST) {
                sprintf(reply, "Usage: dir=<south|north|east|west>");
                return 0;
            }
            return 1;
    }
    return 1;
}

int mcparg_parse_size(char **words, int argpos, char *reply, int *sx, int *sz, int *sy) {
    mcpopt opt_size = {{"size","sz","s"}, argpos,
                       { "%d,%d,%d",
                         "%d,%d",
                         "%d",
                         NULL }};

    int x,z,y;
    int match = mcparg_parse(words, &opt_size, &x, &z, &y);
    switch (match) {
        case 0:
            break;
        case 1:
            y=1;
            break;
        case 2:
            y=1;
            z=x;
            break;
        case MCPARG_NOT_PARSED:
            sprintf(reply, "Usage: size=sx[,sz[,sy]]");
            return 0;
        case MCPARG_NOT_FOUND:
            return 0;
    }

    if (x<=0 || z<=0 || y<=0) {
        sprintf(reply, "Illegal size %d,%d,%d specified",x,z,y);
        return 0;
    }

    *sx = x;
    if (sz) *sz = z;
    if (sy) *sy = y;

    return 1;
}


////////////////////////////////////////////////////////////////////////////////
// Test function

#if TEST

int main(int ac, char **av) {
    char **words = av+1;

    char reply[4096]; reply[0] = 0;
    char buf1[256],buf2[256];

#if 0
    bid_t mat1,mat2;

    if (mcparg_parse_material(words, 0, reply, &mat1, "1")==0) {
        if (reply[0])
            printf("Error parsing material 1: %s\n", reply);
        else
            printf("Material1 not specified\n");

        return 1;
    }

    if (mcparg_parse_material(words, 1, reply, &mat2, "2")==0) {
        if (reply[0])
            printf("Error parsing material 2: %s\n", reply);
        else
            printf("Material2 not specified\n");

        return 2;
    }

    printf("Material1=%d:%d (%s) Material2=%d:%d (%s)\n",
           mat1.bid,mat1.meta,get_bid_name(buf1, mat1),
           mat2.bid,mat2.meta,get_bid_name(buf2, mat2));
#endif

#if 0
    boff_t off = { .dx = 50, .dy = 15, .dz = 40 };
    if (mcparg_parse_offset(words, 0, reply, &off)==0) {
        if (reply[0])
            printf("Error parsing offset: %s\n", reply);
        else
            printf("Offset not specified\n");

        return 1;
    }

    printf("Offset: %d,%d,%d\n",off.dx,off.dz,off.dy);
#endif

#if 0
    int dir = -1;
    if (mcparg_parse_direction(words, 0, reply, &dir)==0) {
        if (reply[0])
            printf("Error parsing direction: %s\n", reply);
        else
            printf("Direction not specified\n");

        return 1;
    }

    printf("Dir: %d %s\n",dir,DIRNAME[dir]);
#endif

    int sx,sz,sy=-1;
    if (mcparg_parse_size(words, 0, reply, &sx, &sz, &sy)==0) {
        if (reply[0])
            printf("Error parsing size: %s\n", reply);
        else
            printf("Size not specified\n");
        return 1;
    }

    printf("Size: %d x %d x %d\n",sx,sz,sy);

    return 0;
}

#endif
