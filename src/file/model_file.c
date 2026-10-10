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

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "params.h"
#include "file/model_file.h"
#include "file/util.h"
#include "log.h"

#define MAX_NETWORKS 8
struct ModelFile_i {
    FILE *fd;
    uint64_t file_size, header_end;
    char language[9];
    char *name, *description;
    ModelType type;
    uint64_t params_offset, params_size;
    size_t num_networks;
    struct { uint64_t offset, size; } networks[MAX_NETWORKS];
};

static bool valid_section(ModelFile model, uint64_t offset, uint64_t size) {
    return size && offset >= model->header_end && offset <= model->file_size &&
        size <= model->file_size - offset && size <= SIZE_MAX;
}

static bool overlaps(uint64_t a, uint64_t an, uint64_t b, uint64_t bn) {
    /* Only used after bounds checks establish that the sums cannot overflow. */
    return a < b + bn && b < a + an;
}

static bool read_header(ModelFile model) {
    if (fseek(model->fd, 0, SEEK_END)) return false;
    long size = ftell(model->fd);
    if (size < 20 || fseek(model->fd, 0, SEEK_SET)) return false;
    model->file_size = (uint64_t)size;
    ModelReader r = {model->fd, model->file_size, true};
    char magic[8];
    if (!mfu_read(&r, magic, 8) || memcmp(magic, "APRILMDL", 8)) return false;
    if (mfu_read_u32(&r) != 1) return false;
    uint64_t header_size = mfu_read_u64(&r);
    if (!r.ok || header_size > r.remaining) return false;
    model->header_end = 20 + header_size;
    r.remaining = header_size;
    if (!mfu_read(&r, model->language, 8)) return false;
    model->name = mfu_alloc_read_string(&r);
    model->description = mfu_alloc_read_string(&r);
    model->type = (ModelType)mfu_read_u32(&r);
    model->params_offset = mfu_read_u64(&r);
    model->params_size = mfu_read_u64(&r);
    uint64_t networks = mfu_read_u64(&r);
    if (!r.ok || !model->name || !model->description ||
        model->type != MODEL_LSTM_TRANSDUCER_STATELESS ||
        networks != LSTM_TRANSDUCER_STATELESS_NETWORK_COUNT ||
        !valid_section(model, model->params_offset, model->params_size)) return false;
    model->num_networks = (size_t)networks;
    for (size_t i = 0; i < model->num_networks; ++i) {
        uint64_t offset = mfu_read_u64(&r), length = mfu_read_u64(&r);
        if (!r.ok || !valid_section(model, offset, length) ||
            overlaps(offset, length, model->params_offset, model->params_size)) return false;
        for (size_t j = 0; j < i; ++j)
            if (overlaps(offset, length, model->networks[j].offset, model->networks[j].size)) return false;
        model->networks[i].offset = offset;
        model->networks[i].size = length;
    }
    return r.ok;
}

ModelFile model_read(const char *path) {
    if (!path) return NULL;
    FILE *fd = fopen(path, "rb");
    if (!fd) return NULL;
    ModelFile model = calloc(1, sizeof(*model));
    if (!model) { fclose(fd); return NULL; }
    model->fd = fd;
    if (!read_header(model)) {
        LOG_WARNING("Invalid or truncated April model header");
        free_model(model);
        return NULL;
    }
    return model;
}

const char *model_name(ModelFile model) { return model->name; }
const char *model_desc(ModelFile model) { return model->description; }
ModelType model_type(ModelFile model) { return model->type; }

bool model_read_params(ModelFile model, ModelParameters *out) {
    if (!model || fseek(model->fd, (long)model->params_offset, SEEK_SET)) return false;
    return read_params_section(out, model->fd, model->params_size);
}

size_t model_network_count(ModelFile model) { return model->num_networks; }
size_t model_network_size(ModelFile model, size_t index) {
    return model && index < model->num_networks ? (size_t)model->networks[index].size : 0;
}
size_t model_network_read(ModelFile model, size_t index, void *data, size_t data_len) {
    size_t size = model_network_size(model, index);
    if (!size || !data) return 0;
    if (data_len > size) data_len = size;
    if (fseek(model->fd, (long)model->networks[index].offset, SEEK_SET)) return 0;
    return fread(data, 1, data_len, model->fd);
}

void transfer_strings_and_free_model(ModelFile model, char **name, char **desc, char **lang) {
    if (!model) return;
    if (model->fd) fclose(model->fd);
    if (name) *name = model->name; else free(model->name);
    if (desc) *desc = model->description; else free(model->description);
    if (lang) {
        *lang = malloc(sizeof(model->language));
        if (*lang) memcpy(*lang, model->language, sizeof(model->language));
    }
    free(model);
}
void free_model(ModelFile model) { transfer_strings_and_free_model(model, NULL, NULL, NULL); }
