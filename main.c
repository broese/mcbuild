#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <gd.h>
#include <gdfontl.h>

#include "anvil.h"

const char * program_name;
int o_level = 256;
int o_help = 0;
const char *o_path = NULL;

// parses argv/argc, 
// sets config variables :
//   o_blocksize, o_digest
int parse_args(int ac, char ** av) {
    int opt,error=0,i;

    program_name = av[0];

    // parse options
    while ( (opt = getopt(ac,av,"l:h")) != -1 ) {
        //printf("opt=%d(%c) optarg=%s optind=%d opterr=%d optopt=%d\n",opt,(char)opt,optarg,optind,opterr,optopt);

        switch (opt) {
            case 'l' : {
                if (sscanf(optarg, "%d", &o_level) != 1) {
                    fprintf(stderr,"%s : cannot parse level %s\n",program_name,optarg);
                    error++;
                }
                break;
            }
            case 'h' : {
                o_help = 1;
                break;
            }
            case '?' : {
                error++;
                break;
            }
        }
    }

    // fetch paths
    o_path = av[optind];
    if ( o_path == NULL ) {
        fprintf(stderr,"%s: missing parameter : file/directory path\n", program_name);
        error++;
    }
    return error;
}

void print_usage() {
    fprintf(stderr,
            "Mine Mapper, Minecraft mapping tool\n\n"

            "Usage: %s [options] path\n\n"


            "Options:\n"
            "  -l <num>          cut-off level\n"
            "  -h                print this help message and quit\n"
            ,program_name);
}

int main(int ac, char **av) {
    
    if ( parse_args(ac,av) || o_help ) {
        print_usage();
        return 1;
    }

    mcregion *region = load_region(o_path);

    ssize_t len;
    unsigned char * data = get_compound_data(region,10,10,&len);
    //write_file("compound.dat", data, len);

    parse_compound(data, len);
    free(data);

    free_region(region);

    return 0;
}
