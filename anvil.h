#pragma once

#include <stdint.h>

#include "nbt.h"

#define REGCHUNKS (32*32)

typedef struct {
    uint8_t   * data[REGCHUNKS];    // compressed chunk data
    ssize_t     len[REGCHUNKS];     // size of each chunk's data
    uint32_t    ts[REGCHUNKS];      // chunk timestamps
} mca;

mca *   anvil_create();
void    anvil_free(mca * region);
void    anvil_dump(mca * region);

mca *   anvil_load(const char *path);
ssize_t anvil_save(mca *region, const char *path);

nbt_t * anvil_get_chunk(mca * region, int32_t X, int32_t Z);
void    anvil_insert_chunk(mca * region, int32_t X, int32_t Z, nbt_t *nbt);
