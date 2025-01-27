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

#ifndef _APRIL_MODEL_IMPL
#define _APRIL_MODEL_IMPL

#include "params.h"

#ifdef __cplusplus
extern "C" {
#endif

struct FBankOptions;

struct april_model_tensors {
    int token_ctx[2];

    float *enc_inp;
    size_t enc_inp_size;

    float *h_state;
    float *c_state;
    float *enc_out;
    float *dec_out;
    float *logits;
    bool requires_decoding;

    bool enc_out_refreshed;
    bool dec_out_refreshed;
};

struct april_model_ggml;
struct april_context_ggml;

struct april_model_ggml *load_model(const char *path);
const char * model_get_name(struct april_model_ggml *model);
const char * model_get_desc(struct april_model_ggml *model);
void model_get_params(struct april_model_ggml *model, ModelParameters *out);
void model_get_fbank_opts(struct april_model_ggml *model, FBankOptions *out);
void model_alloc_tensors(struct april_model_ggml *model, struct april_model_tensors *tensors);

struct april_context_ggml *init_context(struct april_model_ggml *model, int max_batch_size);

bool run_encoder(struct april_context_ggml *ctx, int batch_size, float **inputs, float **h_states, float **c_states, float **outputs);
bool run_decoder(struct april_context_ggml *ctx, int batch_size, int **token_ctx, float **outputs);
bool run_joiner (struct april_context_ggml *ctx, int batch_size, float **enc_out, float **dec_out, float **logits);

// TODO: free_context, free_model
// memory is being leaked currently!

#ifdef __cplusplus
}
#endif


#endif