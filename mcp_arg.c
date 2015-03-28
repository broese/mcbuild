#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>

#include "mcp_arg.h"

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

#if TEST

mcpopt OPT_OFFSET = {
    { "offset", "off", "o", NULL },
    0,
    { "%d,%d,%d", "%d,%d", "%d", NULL },
};

mcpopt OPT_DIR = {
    { "direction", "dir", "d", NULL },
    0,
    { "%s", NULL },
};

int main(int ac, char **av) {
    char **words = av+1;

    int ox=-333,oy=-333,oz=-333;

    switch(mcparg_parse(words, &OPT_OFFSET, &ox, &oz, &oy)) {
        case 0: break;
        case 1: oy=0; break;
        case 2: oz=0; oy=0; break;
        default: {
            char dir[256];
            if (mcparg_parse(words, &OPT_DIR, dir)<0) {
                printf("Usage: #build extend offset=x[,z[,y]]|u|d|r|l|f|b\n");
                return 1;
            }

            switch(dir[0]) {
                case 'u': ox=0;   oz=0;   oy=33;  break;
                case 'd': ox=0;   oz=0;   oy=-33; break;
                case 'r': ox=11;  oz=0;   oy=0;   break;
                case 'l': ox=-11; oz=0;   oy=0;   break;
                case 'f': ox=0;   oz=22;  oy=0;   break;
                case 'b': ox=0;   oz=-22; oy=0;   break;
                default:
                    printf("Usage: #build extend offset=x[,z[,y]]|u|d|r|l|f|b\n");
                    return 1;
            }
        }
    }

    int upper=mcparg_find(words,"upper","up","u","high","h",NULL);

    printf("Offset: %d,%d,%d Upper:%d\n",ox,oz,oy,upper);
    return 0;
}

#endif
