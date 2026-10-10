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
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "common.h"
#include "params.h"
#include "file/util.h"
#include "log.h"

#define ASSERT_OR_RETURN_FALSE(expr) if(!(expr)) { LOG_WARNING("Params: assertion %s failed, line %d", #expr, __LINE__); return false; }

char *get_token(ModelParameters *params, size_t token_index){
    return &params->tokens[params->token_length * token_index];
}

bool read_params(ModelParameters *params, const char *path) {
    if (!path) return false;
    FILE *fd = fopen(path, "rb");
    if (!fd) return false;

    bool result = read_params_from_fd(params, fd);

    fclose(fd);

    return result;
}

bool read_params_from_fd(ModelParameters *params, FILE *fd) {
    if (!fd) return false;
    long start = ftell(fd);
    if (start < 0 || fseek(fd, 0, SEEK_END)) return false;
    long end = ftell(fd);
    if (end < start || fseek(fd, start, SEEK_SET)) return false;
    return read_params_section(params, fd, (uint64_t)(end - start));
}

bool read_params_section(ModelParameters *params, FILE *fd, uint64_t size) {
    ModelReader r = {fd, size, fd != NULL};
    ModelParameters parsed = {0};
    if (!params) return false;
    char magic[8];
    if (!mfu_read(&r, magic, 8) || memcmp(magic, "PARAMS\0\0", 8)) return false;
    parsed.batch_size = mfu_read_i32(&r);
    parsed.segment_size = mfu_read_i32(&r);
    parsed.segment_step = mfu_read_i32(&r);
    parsed.mel_features = mfu_read_i32(&r);
    parsed.sample_rate = mfu_read_i32(&r);
    parsed.frame_shift_ms = mfu_read_i32(&r);
    parsed.frame_length_ms = mfu_read_i32(&r);
    int round_pow2 = mfu_read_i32(&r);
    parsed.round_pow2 = round_pow2 != 0;
    parsed.mel_low = mfu_read_i32(&r);
    parsed.mel_high = mfu_read_i32(&r);
    int snip_edges = mfu_read_i32(&r);
    parsed.snip_edges = snip_edges != 0;
    parsed.token_count = mfu_read_i32(&r);
    parsed.blank_id = mfu_read_i32(&r);
    ASSERT_OR_RETURN_FALSE(r.ok);
    ASSERT_OR_RETURN_FALSE(parsed.batch_size == 1);
    ASSERT_OR_RETURN_FALSE(parsed.segment_size > 0 && parsed.segment_size < 100);
    ASSERT_OR_RETURN_FALSE(parsed.segment_step > 0 && parsed.segment_step <= parsed.segment_size);
    ASSERT_OR_RETURN_FALSE(parsed.mel_features > 0 && parsed.mel_features < 256);
    ASSERT_OR_RETURN_FALSE(parsed.sample_rate > 0 && parsed.sample_rate < 144000);
    ASSERT_OR_RETURN_FALSE(parsed.token_count >= 2 && parsed.token_count < 16384);
    ASSERT_OR_RETURN_FALSE(parsed.blank_id >= 0 && parsed.blank_id < parsed.token_count);
    ASSERT_OR_RETURN_FALSE(parsed.frame_shift_ms > 0 && parsed.frame_shift_ms <= parsed.frame_length_ms);
    ASSERT_OR_RETURN_FALSE(parsed.frame_length_ms > 0 && parsed.frame_length_ms <= 5000);
    int64_t window = (int64_t)parsed.sample_rate * parsed.frame_length_ms / 1000;
    int64_t shift = (int64_t)parsed.sample_rate * parsed.frame_shift_ms / 1000;
    ASSERT_OR_RETURN_FALSE(window >= 2 && window <= 32768 && shift > 0);
    /* PocketFFT packing in the frontend requires an even real FFT size. */
    ASSERT_OR_RETURN_FALSE((round_pow2 == 0 || round_pow2 == 1) && (round_pow2 || window % 2 == 0));
    ASSERT_OR_RETURN_FALSE(snip_edges == 0 || snip_edges == 1);
    ASSERT_OR_RETURN_FALSE(parsed.mel_low >= 0 && parsed.mel_low < parsed.sample_rate / 2);
    ASSERT_OR_RETURN_FALSE(parsed.mel_high == 0 ||
        (parsed.mel_high > parsed.mel_low && parsed.mel_high <= parsed.sample_rate / 2));

    long tokens_start = ftell(fd);
    ASSERT_OR_RETURN_FALSE(tokens_start >= 0);
    uint64_t tokens_remaining = r.remaining;
    /* Bound token width: a corrupt record must not request a huge rectangular
       vocabulary allocation or move the file pointer backwards. */
    for (int i = 0; i < parsed.token_count; ++i) {
        int32_t length = mfu_read_i32(&r);
        ASSERT_OR_RETURN_FALSE(r.ok && length >= 0 && length <= 4096);
        ASSERT_OR_RETURN_FALSE(i == parsed.blank_id || length > 0);
        if ((size_t)length > parsed.token_length) parsed.token_length = (size_t)length;
        ASSERT_OR_RETURN_FALSE(mfu_skip(&r, (uint64_t)length));
    }
    ++parsed.token_length;
    parsed.tokens = calloc((size_t)parsed.token_count, parsed.token_length);
    if (!parsed.tokens) return false;
    r.remaining = tokens_remaining;
    if (fseek(fd, tokens_start, SEEK_SET)) goto fail;
    for (int i = 0; i < parsed.token_count; ++i) {
        int32_t length = mfu_read_i32(&r);
        if (!r.ok || length < 0 || (size_t)length >= parsed.token_length ||
            !mfu_read(&r, get_token(&parsed, i), (size_t)length)) goto fail;
        /* Embedded NULs would silently change a token's visible spelling. */
        if (memchr(get_token(&parsed, i), '\0', (size_t)length)) goto fail;
    }
    *params = parsed;
    return true;
fail:
    free(parsed.tokens);
    return false;
}

void free_params(ModelParameters *params) {
    free(params->tokens);
    params->tokens = NULL;
}
