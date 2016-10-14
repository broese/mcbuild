#include <stdio.h>
#include <stdlib.h>

#include "anvil.h"
#include "nbt.h"

int main(int ac, char **av) {
    if (!av[1]) {
        printf("Usage: %s <region.mca> [X,Z]\n", av[0]);
        exit(1);
    }

    int X=0, Z=0;
    if (av[2]) {
        if (sscanf(av[2], "%u,%u", &X, &Z)!=2 || X>31 || Z>31) {
            printf("Usage: %s <region.mca> [X,Z]\n", av[0]);
            exit(1);
        }
    }

    mca * reg = anvil_load(av[1]);
    if (!reg) {
        printf("Failed to load Anvil file %s\n", av[1]);
        exit(1);
    }

    nbt_t * ch = anvil_get_chunk(reg, X, Z);
    if (!ch) {
        printf("Chunk not found at %s %d,%d\n", av[1], X, Z);
        exit(1);
    }
    nbt_dump(ch);

    return 0;
}

