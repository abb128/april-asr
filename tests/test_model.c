/* Binary parser regressions, including bounds independent of physical EOF. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "april_api.h"
#include "file/model_file.h"
#include "log.h"

static unsigned char file_data[512];
static size_t length;
static void put(uint64_t value, int bytes) {
    for (int i = 0; i < bytes; ++i) { file_data[length++] = value & 255; value >>= 8; }
}
static void patch(size_t offset, uint64_t value, int bytes) {
    for (int i = 0; i < bytes; ++i) { file_data[offset + i] = value & 255; value >>= 8; }
}
static void raw(const char *value, size_t count) {
    memcpy(file_data + length, value, count); length += count;
}
static void write_file(const char *path, size_t count) {
    FILE *f = fopen(path, "wb"); assert(f);
    assert(fwrite(file_data, 1, count, f) == count);
    assert(fclose(f) == 0);
}
static bool read(const char *path) {
    ModelFile model = model_read(path);
    if (!model) return false;
    assert(model_network_size(model, SIZE_MAX) == 0);
    ModelParameters p = {0};
    bool ok = model_read_params(model, &p);
    free_params(&p);
    free_model(model);
    return ok;
}
int main(int argc, char **argv) {
    assert(argc == 2);
    const char *path = argv[1];
    g_loglevel = LEVEL_COUNT;
    raw("APRILMDL", 8); put(1, 4); put(0, 8);
    raw("en\0\0\0\0\0\0", 8);
    put(1, 8); raw("n", 1); put(1, 8); raw("d", 1); put(1, 4);
    size_t param_record = length; put(0, 8); put(0, 8); put(3, 8);
    size_t networks = length;
    for (int i = 0; i < 3; ++i) { put(0, 8); put(1, 8); }
    size_t header_end = length;
    patch(12, header_end - 20, 8);
    for (int i = 0; i < 3; ++i) { patch(networks + i * 16, length, 8); put(0xff, 1); }
    size_t params = length;
    raw("PARAMS\0\0", 8);
    int fields[] = {1, 9, 4, 80, 16000, 10, 25, 1, 20, 0, 1, 2, 0};
    for (size_t i = 0; i < sizeof(fields) / sizeof(*fields); ++i) put(fields[i], 4);
    size_t first_token = length;
    put(5, 4); raw("<blk>", 5); put(1, 4); raw("A", 1);
    patch(param_record, params, 8); patch(param_record + 8, length - params, 8);
    unsigned char valid[512]; memcpy(valid, file_data, sizeof(valid));
    write_file(path, length); assert(read(path));
    assert(!model_read(NULL));
    ModelParameters empty = {0};
    assert(!read_params(&empty, "this-model-does-not-exist.april"));
    assert(!read_params_from_fd(&empty, NULL));
    for (size_t count = 0; count < length; ++count) {
        write_file(path, count); assert(!read(path));
    }
    struct { size_t offset; uint64_t value; int bytes; } mutations[] = {
        {12, UINT64_MAX, 8}, {12, 12, 8}, {28, UINT64_MAX, 8},
        {param_record, UINT64_MAX - 10, 8}, {param_record + 8, UINT64_MAX, 8},
        {param_record + 8, 8, 8}, /* Rest of the file exists: respect section bounds. */
        {networks, 20, 8}, {networks, params, 8},
        {networks + 16, header_end, 8}, {networks + 8, UINT64_MAX, 8},
        {first_token, UINT32_MAX, 4}, {first_token, 5000, 4},
        {first_token + 9, 0, 4}, /* Empty nonblank token. */
        {params + 8 + 5 * 4, 0, 4}, {params + 8 + 6 * 4, 5000, 4},
        {params + 8 + 7 * 4, 2, 4}, {params + 8 + 9 * 4, 9000, 4}
    };
    for (size_t i = 0; i < sizeof(mutations) / sizeof(*mutations); ++i) {
        memcpy(file_data, valid, sizeof(valid));
        patch(mutations[i].offset, mutations[i].value, mutations[i].bytes);
        write_file(path, length); assert(!read(path));
    }
    /* Deterministic mutations exercise cleanup of partially allocated headers. */
    uint32_t state = 77;
    for (int i = 0; i < 1000; ++i) {
        memcpy(file_data, valid, sizeof(valid));
        state = state * 1664525u + 1013904223u;
        file_data[state % length] ^= (unsigned char)(state >> 24) | 1;
        write_file(path, length);
        (void)read(path);
    }
    memcpy(file_data, valid, sizeof(valid));
    write_file(path, length);
    aam_api_init(APRIL_VERSION);
    /* Valid container, invalid ONNX: return NULL without terminating the host. */
    assert(!aam_create_model(path));
    assert(remove(path) == 0);
    return 0;
}
