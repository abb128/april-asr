#ifndef _APRIL_MODEL_FILE_UTIL
#define _APRIL_MODEL_FILE_UTIL

#include <stdio.h>

static inline uint32_t mfu_read_u32(FILE *fd) {
    uint32_t v;
    fread(&v, sizeof(uint32_t), 1, fd);
    v = le32toh(v);
    return v;
}

static inline uint64_t mfu_read_u64(FILE *fd) {
    uint64_t v;
    fread(&v, sizeof(uint64_t), 1, fd);
    v = le64toh(v);
    return v;
}

static inline int32_t mfu_read_i32(FILE *fd) {
    uint32_t v;
    fread(&v, sizeof(uint32_t), 1, fd);
    v = le32toh(v);
    return *((int32_t *)&v);
}

static inline int64_t mfu_read_i64(FILE *fd) {
    uint64_t v;
    fread(&v, sizeof(uint64_t), 1, fd);
    v = le64toh(v);
    return *((int64_t *)&v);
}

// Must be freed manually with free(v)
static inline char *mfu_alloc_read_string(FILE *fd) {
    uint64_t size = mfu_read_u64(fd);
    char *v = (char *)calloc(1, size + 1);
    if(v == NULL) {
        printf("mfu: failed allocating string of size %llu, file position %llu\n", size, ftell(fd));
        assert(false);
    }
    fread(v, 1, size, fd);
    v[size] = '\0';
    return v;
}

#endif