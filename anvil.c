#include "anvil.h"
#include "nbt.h"

#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __linux
#define HAS_ENDIAN 1
#endif

#ifndef HAS_ENDIAN
#include <sys/types.h>
#include <netinet/in.h>
#include <inttypes.h>
#define be32toh(x) ntohl(x)
#else
#include <endian.h>
#endif

#include "lh_debug.h"
#include "lh_files.h"
#include "lh_buffers.h"
#include "lh_compress.h"


mcregion * load_region(const char * path) {
    // open file
    ssize_t size;
    FILE *fd = open_file_r(path, &size);
    if (!fd) LH_ERROR(NULL,"load_region : failed to open file %s", path);
    printf("Opened %s (%u KiB, %u pages)\n",path,size/1024,size/4096);

    // allocate region
    ALLOC(mcregion,region);
    region->fd = fd;
    region->path = path;
    region->size = size/4096;

    // load column (compound) table
    int i;
    uint32_t table[4096];
    if (read_from(fd, 0, (unsigned char *)table, sizeof(table))<0)
        return NULL;

    // allocate column table
    ALLOCNE(region->columns, 1024);

    int gen = 0; // number of generated columns

    for(i=0; i<1024; i++) {
        uint32_t c = be32toh(table[i]);      // column info from the column table
        mccolumn * col = region->columns+i;
        col->sectors = c&0xff;               // number of 4k sectors/pages of the column data
        col->offset = (off_t)(c>>8) * 4096;  // starting offset of the column data in the MCA file


        if (col->sectors > 0) {              // this index is non-empty
            struct {
                uint32_t size;
                char     compr;
            } ch;
            
            read_from(fd, col->offset, (unsigned char *)&ch, sizeof(ch));
            col->size = be32toh(ch.size);    // exact size of the column data in bytes
            col->compr = ch.compr;           // compression type

            if (col->size>0) gen++;          // is size>0, column data is already generated
            //printf("%4d %08x %2d %08x %10d %d\n",i,c,col->sectors,col->offset,col->size,col->compr);
        }

    }
    printf("Loaded column table, %d columns generated (%.2f%%)\n",
           gen, (float)gen/10.24);

    return region;
}

void free_region(mcregion *region) {
    if (!region) return;

    if (region->columns)
    fclose(region->fd);
    free(region);
}

unsigned char * get_compound_data(mcregion *region, int idx, ssize_t *len) {
    mccolumn * col = &region->columns[idx];
    if (col->size <= 0) return NULL;

    unsigned char *cdata = read_froma(region->fd, col->offset+5, col->size);
    if (!cdata) return NULL;

    unsigned char * data = zlib_decode(cdata, col->size, len);
    free(cdata);
    return data;
}

////////////////////////////////////////////////////////////////////////////////

