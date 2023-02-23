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

#ifndef _APRIL_PARAMS
#define _APRIL_PARAMS

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "common.h"

typedef struct ModelParameters {
    int batch_size;
    int segment_size;
    int segment_step;
    int mel_features;
    int sample_rate;

    int frame_shift_ms;
    int frame_length_ms;
    bool round_pow2;
    int mel_low;
    int mel_high;
    bool snip_edges;

    int blank_id;

    int token_count;
    size_t token_length;
    
    char *tokens;
} ModelParameters;

char *get_token(ModelParameters *params, size_t token_index);

// Returns false if reading failed
bool read_params(ModelParameters *params, const char *path);
bool read_params_from_fd(ModelParameters *params, FILE *fd);

void free_params(ModelParameters *params);

#endif