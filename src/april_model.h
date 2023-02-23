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

#ifndef _APRIL_MODEL
#define _APRIL_MODEL

#include "common.h"
#include "ort_util.h"
#include "april_api.h"
#include "params.h"
#include "fbank.h"
struct AprilASRModel_i {
    OrtEnv *env;
    OrtSessionOptions* session_options;

    OrtSession* encoder;
    OrtSession* decoder;
    OrtSession* joiner;

    // The comment numbers are for reference only, it may differ
    // with different sized models.
    int64_t x_dim[3];       // (1, 9, 80)
    int64_t h_dim[3];       // (12, 1, 512)
    int64_t c_dim[3];       // (12, 1, 1024)
    int64_t eout_dim[3];    // (1, 1, 512)
    int64_t dout_dim[3];    // (1, 1, 512)
    int64_t context_dim[2]; // (1, 2)
    int64_t logits_dim[3];  // (1, 1, 500)

    FBankOptions fbank_opts;
    ModelParameters params;

    char *name;
    char *description;
    char *language;
};

#endif