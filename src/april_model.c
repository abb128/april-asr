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

#include <stdlib.h>
#include "common.h"
#include "file/model_file.h"
#include "april_model.h"
#include "log.h"
#include "april_session.h"
#include "fbank.h"
#include "proc_thread.h"

#include <errno.h>
#include <time.h>

#define ASSERT_OR_RETURN_NULL(expr) if(!(expr)) { LOG_WARNING("Model: assertion " #expr " failed, line %d", __LINE__); return NULL; }
#define ASSERT_OR_FREE_AAM_AND_RETURN_NULL(aam, expr) if(!(expr)) { LOG_WARNING("Model: assertion " #expr " failed, line %d", __LINE__); aam_free(aam); return NULL; }
AprilASRModel aam_create_model(const char *model_path) {
    struct april_model_ggml *model = load_model(model_path);
    if(model == NULL) {
        LOG_ERROR("aam: failed to load %s as a gguf file", model_path);
        // TODO: Tell the user about the file format change
        return NULL;
    }

    AprilASRModel aam = (AprilASRModel)calloc(1, sizeof(struct AprilASRModel_i));
    aam->model = model;
    model_get_params(model, &aam->params);

    aam->fbank_opts.sample_freq        = aam->params.sample_rate;
    aam->fbank_opts.num_bins           = aam->params.mel_features;
    aam->fbank_opts.pull_segment_count = aam->params.segment_size;
    aam->fbank_opts.pull_segment_step  = aam->params.segment_step;
    aam->fbank_opts.frame_shift_ms     = aam->params.frame_shift_ms;
    aam->fbank_opts.frame_length_ms    = aam->params.frame_length_ms;
    aam->fbank_opts.round_pow2         = aam->params.round_pow2;
    aam->fbank_opts.mel_low            = aam->params.mel_low;
    aam->fbank_opts.mel_high           = aam->params.mel_high;
    aam->fbank_opts.preemph_coeff      = 0.97f;
    aam->fbank_opts.remove_dc_offset   = true;
    aam->fbank_opts.snip_edges         = true;//aam->params.snip_edges;

    aam->ctx = init_context(aam->model, MAX_SESSIONS);
    LOG_INFO("aam: loaded model %s", aam_get_name(aam));

    return aam;
}

const char *aam_get_name(AprilASRModel model) { return model_get_name(model->model); }
const char *aam_get_description(AprilASRModel model) { return model_get_desc(model->model); }
const char *aam_get_language(AprilASRModel model) { return "TODO"; }

size_t aam_get_sample_rate(AprilASRModel model) {
    return model->fbank_opts.sample_freq;
}


void aam_pt_raise_flag(AprilASRModel model) {
    aam_ensure_proc_thread_spawned(model);
    pt_raise(model->proc_thread, PT_FLAG_AUDIO);
}


int aam_collect_loop(AprilASRModel model, AprilASRSession force_session) {
    int num_encoded = 0;
    bool any_inferred = true;

    while(any_inferred) {
        any_inferred = false;
        if(aam_collect_and_encode(model, force_session) > 0) {
            num_encoded += 1;
            any_inferred = true;
        }

        // limit max 4 tokens per encode
        for(int i=0; i<4; i++) {
            aam_collect_and_decode(model, force_session);
            if(!aam_collect_and_join(model, force_session)) break;
        }
    }

    return num_encoded;
}

void aam_proc_thread_callback(void *model_ptr, int flag) {
    AprilASRModel model = (AprilASRModel)model_ptr;

    // TODO: Sonic speedup is currently disabled

    //clock_t clock_start = clock();
    
    for(int i=0; i<MAX_SESSIONS; i++) {
        AprilASRSession aas = model->sessions[i];
        if(aas == NULL) continue;
        if(aas->sync) continue;

        aas_run_self_pt_tasks(aas);
    }

    //clock_t clock_mid = clock();

    aam_collect_loop(model, NULL);

    //clock_t clock_end = clock();

    //double time_self_pt_tasks = ((double)(clock_mid - clock_start) * 1000.0) / ((double)CLOCKS_PER_SEC);
    //double time_collect = ((double)(clock_end - clock_mid) * 1000.0) / ((double)CLOCKS_PER_SEC);

    //printf("pt tasks:    %2.2f ms\n", time_self_pt_tasks);
    //printf("collect:     %2.2f ms\n", time_collect);
}

int aam_collect_and_encode(AprilASRModel model, AprilASRSession force_session) {
    AprilASRSession sessions_need_encoding[MAX_SESSIONS];
    size_t batch_size = 0;
    for(int i=0; i<MAX_SESSIONS; i++) {
        if(model->sessions[i] == NULL) continue;
        if(force_session && model->sessions[i] != force_session) continue;
        if(model->sessions[i]->sync && !force_session) continue;

        if(fbank_pull_segments(
            model->sessions[i]->fbank,
            model->sessions[i]->tensors.enc_inp,
            model->sessions[i]->tensors.enc_inp_size
        )) {
            sessions_need_encoding[batch_size++] = model->sessions[i];
        }
    }

    if(batch_size == 0) return 0;

    float *inputs  [MAX_SESSIONS];
    float *h_states[MAX_SESSIONS];
    float *c_states[MAX_SESSIONS];
    float *outputs [MAX_SESSIONS];
    for(int i=0; i<batch_size; i++){
        inputs[i] = sessions_need_encoding[i]->tensors.enc_inp;
        h_states[i] = sessions_need_encoding[i]->tensors.h_state;
        c_states[i] = sessions_need_encoding[i]->tensors.c_state;
        outputs[i] = sessions_need_encoding[i]->tensors.enc_out;
    }

    bool result = run_encoder(model->ctx, batch_size, inputs, h_states, c_states, outputs);
    if(!result) return -1;
    for(int i=0; i<batch_size; i++){
        sessions_need_encoding[i]->tensors.enc_out_refreshed = true;
    }

    return batch_size;
}

int aam_collect_and_decode(AprilASRModel model, AprilASRSession force_session) {
    AprilASRSession sessions_need_decoding[MAX_SESSIONS];
    size_t batch_size = 0;
    for(int i=0; i<MAX_SESSIONS; i++) {
        if(model->sessions[i] == NULL) continue;
        if(force_session && model->sessions[i] != force_session) continue;
        if(model->sessions[i]->sync && !force_session) continue;

        if(model->sessions[i]->tensors.requires_decoding) {
            sessions_need_decoding[batch_size++] = model->sessions[i];
            model->sessions[i]->tensors.requires_decoding = false;
        }
    }

    if(batch_size == 0) return 0;

    int *token_ctx[MAX_SESSIONS];
    float *outputs[MAX_SESSIONS];
    for(int i=0; i<batch_size; i++){
        token_ctx[i] = sessions_need_decoding[i]->tensors.token_ctx;
        outputs[i] = sessions_need_decoding[i]->tensors.dec_out;
    }

    bool result = run_decoder(model->ctx, batch_size, token_ctx, outputs);
    if(!result) return -1;
    for(int i=0; i<batch_size; i++){
        sessions_need_decoding[i]->tensors.dec_out_refreshed = true;
    }

    return batch_size;
}

int aam_collect_and_join(AprilASRModel model, AprilASRSession force_session) {
    AprilASRSession sessions_need_joining[MAX_SESSIONS];
    size_t batch_size = 0;
    for(int i=0; i<MAX_SESSIONS; i++) {
        if(model->sessions[i] == NULL) continue;
        if(force_session && model->sessions[i] != force_session) continue;
        if(model->sessions[i]->sync && !force_session) continue;

        if(model->sessions[i]->tensors.enc_out_refreshed || model->sessions[i]->tensors.dec_out_refreshed) {
            sessions_need_joining[batch_size++] = model->sessions[i];
        }
    }

    if(batch_size == 0) return 0;

    float *enc_out[MAX_SESSIONS];
    float *dec_out[MAX_SESSIONS];
    float *logits[MAX_SESSIONS];
    for(int i=0; i<batch_size; i++){
        enc_out[i] = sessions_need_joining[i]->tensors.enc_out;
        dec_out[i] = sessions_need_joining[i]->tensors.dec_out;
        logits[i]  = sessions_need_joining[i]->tensors.logits;
    }

    bool result = run_joiner(model->ctx, batch_size, enc_out, dec_out, logits);
    if(!result) return -1;
    for(int i=0; i<batch_size; i++){
        sessions_need_joining[i]->tensors.enc_out_refreshed = false;
        sessions_need_joining[i]->tensors.dec_out_refreshed = false;
    }

    for(int i=0; i<batch_size; i++){
        // TODO: Early emit is currently disabled
        aas_process_logits(sessions_need_joining[i], 0.0f);
    }

    return batch_size;
}

void aam_free(AprilASRModel model) {
    if(model == NULL) return;

    // TODO: Free model->model, it is being leaked right now
    free_params(&model->params);

    free(model);
}

void aam_ensure_proc_thread_spawned(AprilASRModel model) {
    if(model->proc_thread != NULL) return;

    model->proc_thread = pt_create(aam_proc_thread_callback, model);
}
