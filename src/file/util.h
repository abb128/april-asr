/*
 * Copyright (C) 2022 abb128
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef _APRIL_MODEL_FILE_UTIL
#define _APRIL_MODEL_FILE_UTIL

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

/* All reads are confined to the declared section, including skips and strings.
   Decode explicitly so short reads and unaligned/type-punned loads are safe. */
typedef struct ModelReader {
    FILE *fd;
    uint64_t remaining;
    bool ok;
} ModelReader;

static inline bool mfu_read(ModelReader *r, void *out, size_t size) {
    if (!r->ok || size > r->remaining || fread(out, 1, size, r->fd) != size) {
        r->ok = false;
        return false;
    }
    r->remaining -= size;
    return true;
}

static inline bool mfu_skip(ModelReader *r, uint64_t size) {
    if (!r->ok || size > r->remaining || size > LONG_MAX || fseek(r->fd, (long)size, SEEK_CUR)) {
        r->ok = false;
        return false;
    }
    r->remaining -= size;
    return true;
}

static inline uint32_t mfu_read_u32(ModelReader *r) {
    unsigned char v[4] = {0};
    if (!mfu_read(r, v, sizeof(v))) return 0;
    return (uint32_t)v[0] | (uint32_t)v[1] << 8 | (uint32_t)v[2] << 16 | (uint32_t)v[3] << 24;
}

static inline uint64_t mfu_read_u64(ModelReader *r) {
    uint64_t low = mfu_read_u32(r);
    uint64_t high = mfu_read_u32(r);
    return low | high << 32;
}

static inline int32_t mfu_read_i32(ModelReader *r) {
    uint32_t bits = mfu_read_u32(r);
    int32_t value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static inline char *mfu_alloc_read_string(ModelReader *r) {
    uint64_t size = mfu_read_u64(r);
    /* Metadata is text, not network weights; cap individual strings at 1 MiB. */
    if (!r->ok || size > r->remaining || size > 1024 * 1024) {
        r->ok = false;
        return NULL;
    }
    char *value = malloc((size_t)size + 1);
    if (!value || !mfu_read(r, value, (size_t)size)) {
        free(value);
        r->ok = false;
        return NULL;
    }
    value[size] = '\0';
    return value;
}

#endif
