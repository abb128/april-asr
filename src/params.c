/*
 * Copyright (C) 2025 abb128
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

#define ASSERT_OR_RETURN_FALSE(expr) if(!(expr)) { LOG_WARNING("Params: assertion " #expr " failed, line %d", __LINE__); return false; }

const char *get_token(ModelParameters *params, size_t token_index){
    return &params->tokens[params->token_length * token_index];
}

bool read_params(ModelParameters *params, const char *path) {
    FILE *fd = fopen(path, "r");

    bool result = read_params_from_fd(params, fd);

    fclose(fd);

    return result;
}

const char *PARAMS_EXPECTED_MAGIC = "PARAMS\0\0";
bool read_params_from_fd(ModelParameters *params, FILE *fd) {
    char magic[8];
    fread(magic, 1, 8, fd);

    if(memcmp(magic, PARAMS_EXPECTED_MAGIC, 8) != 0) {
        LOG_INFO("magic check failed for params");
        return false;
    }

    params->batch_size   = mfu_read_i32(fd);
    params->segment_size = mfu_read_i32(fd);
    params->segment_step = mfu_read_i32(fd);
    params->mel_features = mfu_read_i32(fd);
    params->sample_rate  = mfu_read_i32(fd);

    params->frame_shift_ms  = mfu_read_i32(fd);
    params->frame_length_ms = mfu_read_i32(fd);
    params->round_pow2      = mfu_read_i32(fd) != 0;
    params->mel_low         = mfu_read_i32(fd);
    params->mel_high        = mfu_read_i32(fd);
    params->snip_edges      = mfu_read_i32(fd) != 0;

    params->token_count  = mfu_read_i32(fd);
    params->blank_id     = mfu_read_i32(fd);

    ASSERT_OR_RETURN_FALSE(params->batch_size == 1);
    ASSERT_OR_RETURN_FALSE((params->segment_size > 0) && (params->segment_size < 100));
    ASSERT_OR_RETURN_FALSE((params->segment_step > 0) && (params->segment_step < 100) && (params->segment_step <= params->segment_size));
    ASSERT_OR_RETURN_FALSE((params->mel_features > 0) && (params->mel_features < 256));
    ASSERT_OR_RETURN_FALSE((params->sample_rate > 0) && (params->sample_rate < 144000));
    ASSERT_OR_RETURN_FALSE((params->token_count > 0) && (params->token_count < 16384));
    ASSERT_OR_RETURN_FALSE((params->blank_id >= 0) && (params->blank_id < params->token_count));

    ASSERT_OR_RETURN_FALSE((params->frame_shift_ms > 0) && (params->frame_shift_ms <= params->frame_length_ms));
    ASSERT_OR_RETURN_FALSE((params->frame_length_ms > 0) && (params->frame_length_ms <= 5000));
    ASSERT_OR_RETURN_FALSE((params->mel_low > 0) && (params->mel_low < params->sample_rate));
    ASSERT_OR_RETURN_FALSE((params->mel_high == 0) || (params->mel_high > params->mel_low));


    // Read all piece lengths and figure out the maximum
    size_t tokens_start = ftell(fd);

    params->token_length = 0;
    for(int i=0; i<params->token_count; i++){
        int32_t token_len = mfu_read_i32(fd);
        if(token_len > (int32_t)params->token_length)
            params->token_length = token_len;

        fseek(fd, token_len, SEEK_CUR);
    }
    params->token_length += 1; // for '\0' byte

    // Allocate the memory
    params->tokens = (char *)calloc(params->token_count, params->token_length);

    // Rewind back and read
    fseek(fd, tokens_start, SEEK_SET);
    for(int i=0; i<params->token_count; i++){
        int32_t token_len = mfu_read_i32(fd);

        ASSERT_OR_RETURN_FALSE(token_len < (int32_t)params->token_length);

        fread(get_token(params, i), 1, token_len, fd);
    }

    return true;
}

void free_params(ModelParameters *params){
    free(params->tokens);
}
