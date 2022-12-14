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

#include "log.h"
#include "params.h"
#include "april_session.h"

void run_aas_callback(void *userdata, int flags);

AprilASRSession aas_create_session(AprilASRModel model, AprilConfig config) {
    AprilASRSession aas = (AprilASRSession)calloc(1, sizeof(struct AprilASRSession_i));

    aas->model = model;
    aas->fbank = make_fbank(model->fbank_opts);

    ORT_ABORT_ON_ERROR(g_ort->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &aas->memory_info));
    OrtMemoryInfo *mi = aas->memory_info;

    aas->x = alloc_tensor3f(mi, model->x_dim);
    for(int i=0; i<2; i++){
        aas->h[i] = alloc_tensor3f(mi, model->h_dim);
        aas->c[i] = alloc_tensor3f(mi, model->c_dim);
    }

    aas->eout = alloc_tensor3f(mi, model->eout_dim);
    aas->dout = alloc_tensor3f(mi, model->dout_dim);

    aas->context = alloc_tensor2i(mi, model->context_dim);

    if(model->context_dim[0] != 1) {
        LOG_ERROR("Currently, only batch size 1 is supported. Got batch size %ld", model->context_dim[0]);
        aas_free(aas);
        return NULL;
    }
    aas->context_size = model->context_dim[1];

    aas->logits = alloc_tensor3f(mi, model->logits_dim);

    aas->dout_init = false;
    aas->hc_use_0 = false;
    aas->active_token_head = 0;

    aas->was_flushed = false;
    aas->runs_since_emission = 100;

    assert(aas->fbank          != NULL);
    assert(aas->x.tensor       != NULL);
    assert(aas->h[0].tensor    != NULL);
    assert(aas->c[0].tensor    != NULL);
    assert(aas->h[1].tensor    != NULL);
    assert(aas->c[1].tensor    != NULL);
    assert(aas->eout.tensor    != NULL);
    assert(aas->context.tensor != NULL);
    assert(aas->logits.tensor  != NULL);

    aas->handler = config.handler;
    aas->userdata = config.userdata;
    aas->sync = (config.flags & ARPIL_CONFIG_FLAG_SYNCHRONOUS_BIT) != 0;
    if(aas->handler == NULL) {
        LOG_ERROR("No handler provided! A handler is required, please provide a handler");
        aas_free(aas);
        return NULL;
    }

    if(!aas->sync){
        aas->provider = ap_create();
        aas->thread = pt_create(run_aas_callback, aas);
    }

    return aas;
}

void aas_free(AprilASRSession session) {
    if(session == NULL) return;

    pt_free(session->thread);
    ap_free(session->provider);

    free_tensorf(&session->logits);
    free_tensori(&session->context);
    free_tensorf(&session->eout);
    free_tensorf(&session->dout);
    for(int i=0; i<2; i++) {
        free_tensorf(&session->c[i]);
        free_tensorf(&session->h[i]);
    }

    free_tensorf(&session->x);
    g_ort->ReleaseMemoryInfo(session->memory_info);
    free_fbank(session->fbank);

    free(session);
}

const char* encoder_input_names[] = {"x", "h", "c"};
const char* encoder_output_names[] = {"encoder_out", "next_h", "next_c"};

const char* decoder_input_names[] = {"context"};
const char* decoder_output_names[] = {"decoder_out"};

const char* joiner_input_names[] = {"encoder_out", "decoder_out"};
const char* joiner_output_names[] = {"logits"};

// Runs encoder on current data in aas->x
void aas_run_encoder(AprilASRSession aas){
    aas->hc_use_0 = !aas->hc_use_0;
    const OrtValue *inputs[] = {
        aas->x.tensor,
        aas->h[aas->hc_use_0 ? 0 : 1].tensor,
        aas->c[aas->hc_use_0 ? 0 : 1].tensor
    };

    OrtValue *outputs[] = {
        aas->eout.tensor,
        aas->h[aas->hc_use_0 ? 1 : 0].tensor,
        aas->c[aas->hc_use_0 ? 1 : 0].tensor
    };

    ORT_ABORT_ON_ERROR(g_ort->Run(aas->model->encoder, NULL,
                                    encoder_input_names, inputs, 3,
                                    encoder_output_names, 3, outputs));
}

// Runs decoder on current data in aas->context
void aas_run_decoder(AprilASRSession aas){
    const OrtValue *inputs[] = {
        aas->context.tensor
    };

    OrtValue *outputs[] = {
        aas->dout.tensor
    };

    ORT_ABORT_ON_ERROR(g_ort->Run(aas->model->decoder, NULL,
                                    decoder_input_names, inputs, 1,
                                    decoder_output_names, 1, outputs));
}

// Runs joiner on current data in aas->eout and aas->dout
void aas_run_joiner(AprilASRSession aas){
    const OrtValue *inputs[] = {
        aas->eout.tensor,
        aas->dout.tensor
    };

    OrtValue *outputs[] = {
        aas->logits.tensor
    };

    ORT_ABORT_ON_ERROR(g_ort->Run(aas->model->joiner, NULL,
                                    joiner_input_names, inputs, 2,
                                    joiner_output_names, 1, outputs));
}

void aas_update_context(AprilASRSession aas, int64_t new_token){
    if(aas->context_size == 2) {
        aas->context.data[0] = aas->context.data[1];
        aas->context.data[1] = new_token;
    } else {
        size_t last_idx = aas->context_size - 1;
        memmove(
            &aas->context.data[0], &aas->context.data[1],
            last_idx * sizeof(int64_t)
        );

        aas->context.data[last_idx] = new_token;
    }

    aas_run_decoder(aas);
}


void aas_finalize_tokens(AprilASRSession aas) {
    if(aas->active_token_head  == 0) return;

    aas->handler(
        aas->userdata,
        APRIL_RESULT_RECOGNITION_FINAL,
        aas->active_token_head,
        aas->active_tokens
    );

    aas->last_handler_call_head = aas->active_token_head;
    aas->active_token_head = 0;
}

bool aas_emit_token(AprilASRSession aas, AprilToken *new_token, bool force){
    if(new_token != NULL) {
        if((!force) && (aas->last_handler_call_head == (aas->active_token_head + 1))
            && (aas->active_tokens[aas->active_token_head].token == new_token->token)
        ) {
            return false;
        }

        aas->active_tokens[aas->active_token_head++] = *new_token;
    } else {
        if((!force) && (aas->last_handler_call_head == (aas->active_token_head))) {
            return false;
        }
    }

    aas->handler(
        aas->userdata,
        APRIL_RESULT_RECOGNITION_PARTIAL,
        aas->active_token_head,
        aas->active_tokens
    );

    aas->last_handler_call_head = aas->active_token_head;
    return true;
}

// Processes current data in aas->logits. Returns true if new token was
// added, else returns false if no new data is available. Updates
// aas->context and aas->active_tokens. Uses basic greedy search algorithm.
bool aas_process_logits(AprilASRSession aas, float early_emit){
    ModelParameters *params = &aas->model->params;
    size_t blank = params->blank_id;
    float *logits = aas->logits.data;

    int max_idx = -1;
    float max_val = -9999999999.0;
    for(size_t i=0; i<(size_t)params->token_count; i++){
        if(i == blank) continue;

        if(logits[i] > max_val){
            max_idx = i;
            max_val = logits[i];
        }
    }

    // If the current token is equal to previous, ignore early_emit.
    // Helps prevent repeating like ALUMUMUMUMUMUININININIUM which happens for some reason
    bool is_equal_to_previous = aas->context.data[1] == max_idx;
    if(is_equal_to_previous) early_emit = 0.0f;

    // If no emissions in a while, ignore early_emit.
    // Helps prevent starting with stray " I" or similar phenomena
    //if(aas->runs_since_emission > 90) early_emit = 0.0f;

    float blank_val = logits[blank];
    bool is_blank = (blank_val - early_emit) > max_val;


    AprilToken token = { get_token(params, max_idx), max_val };

    // works for English and other latin languages, may need to do something
    // different here for other languages like Chinese
    if (token.token[0] == ' ') token.flags |= APRIL_TOKEN_FLAG_WORD_BOUNDARY_BIT;

    // If current token is non-blank, emit and return
    if(!is_blank) {
        aas->runs_since_emission = 0;

        aas_update_context(aas, (int64_t)max_idx);

        bool is_final = (aas->active_token_head >= (MAX_ACTIVE_TOKENS - 2))
            || ((token.flags & APRIL_TOKEN_FLAG_WORD_BOUNDARY_BIT)
                && (aas->active_token_head >= (MAX_ACTIVE_TOKENS / 2)));

        if(is_final) aas_finalize_tokens(aas);
        aas_emit_token(aas, &token, true);
    } else {
        aas->runs_since_emission += 1;

        // If there's been silence for a while, forcibly reduce confidence to
        // kill stray prediction
        max_val -= (float)(aas->runs_since_emission-1)/10.0f;

        // If current token is blank, but it's reasonably confident, emit
        bool reasonably_confident = (!is_equal_to_previous) && (max_val > (blank_val - 4.0f));
        bool been_bit_long_silence = aas->runs_since_emission >= 20; // TODO: Use a precise number like 1 seconds
        bool been_long_silence = aas->runs_since_emission >= 50; // TODO: Use a precise number like 2.2 seconds

        if(been_bit_long_silence && (!been_long_silence)){
            // in case reasonably confident prediction was output previously
            aas_emit_token(aas, NULL, false);
        }else if(reasonably_confident) {
            token.logprob -= 8.0;
            if(aas_emit_token(aas, &token, false)) {
                assert(aas->active_token_head > 0);
                aas->active_token_head--;
            }
        } else if (been_long_silence) {
            aas_finalize_tokens(aas);
        }
    }

    return is_blank;
}

bool aas_infer(AprilASRSession aas){
    if(!aas->dout_init) {
        for(size_t i=0; i<aas->context_size; i++) {
            aas_update_context(aas, aas->model->params.blank_id);
        }

        aas->dout_init = true;
    }

    bool any_inferred = false;
    while(fbank_pull_segments( aas->fbank, aas->x.data, sizeof(float)*SHAPE_PRODUCT3(aas->model->x_dim) )){
        aas_run_encoder(aas);

        float early_emit = 1.5f;
        for(int i=0; i<8; i++){
            early_emit -= 1.0f;
            aas_run_joiner(aas);
            if(aas_process_logits(aas, early_emit > 0.0f ? early_emit : 0.0f)) break;
        }

        any_inferred = true;
    }

    return any_inferred;
}

void _aas_feed_pcm16(AprilASRSession session, short *pcm16, size_t short_count);
void aas_feed_pcm16(AprilASRSession session, short *pcm16, size_t short_count) {
    if(session->sync) return _aas_feed_pcm16(session, pcm16, short_count);

    bool success = ap_push_audio(session->provider, pcm16, short_count);
    pt_raise(session->thread, PT_FLAG_AUDIO);

    if(!success){
        session->handler(
            session->userdata,
            APRIL_RESULT_ERROR_CANT_KEEP_UP,
            0,
            NULL
        );
    }
}


#ifdef APRIL_DEBUG_SAVE_AUDIO
FILE *fd = NULL;
#endif

#define SEGSIZE 3200 //TODO
void _aas_feed_pcm16(AprilASRSession session, short *pcm16, size_t short_count) {
#ifdef APRIL_DEBUG_SAVE_AUDIO
    if(fd == NULL) fd = fopen("/tmp/aas_debug.bin", "w");
#endif

    assert(session->fbank != NULL);
    assert(session != NULL);
    assert(pcm16 != NULL);

    session->was_flushed = false;

    size_t head = 0;
    float wave[SEGSIZE];

    while(head < short_count){
        size_t remaining = short_count - head;
        if(remaining < 1) break;
        if(remaining > SEGSIZE) remaining = SEGSIZE;

        for(size_t i=0; i<remaining; i++){
            wave[i] = (float)pcm16[head + i] / 32768.0f;
        }

#ifdef APRIL_DEBUG_SAVE_AUDIO
        fwrite(wave, sizeof(float), remaining, fd);
#endif

        fbank_accept_waveform(session->fbank, wave, remaining);

        aas_infer(session);

        head += remaining;
    }

#ifdef APRIL_DEBUG_SAVE_AUDIO
    fflush(fd);
#endif
}

void _aas_flush(AprilASRSession session);
void aas_flush(AprilASRSession session) {
    if(session->sync) return _aas_flush(session);

    pt_raise(session->thread, PT_FLAG_FLUSH);
}

const float ZEROS[SEGSIZE] = { 0 };
void _aas_flush(AprilASRSession session) {
    if(session->was_flushed) return;

    session->was_flushed = true;

    while(fbank_flush(session->fbank))
        aas_infer(session);

    for(int i=0; i<2; i++)
        fbank_accept_waveform(session->fbank, ZEROS, SEGSIZE);

    while(fbank_flush(session->fbank))
        aas_infer(session);

    aas_finalize_tokens(session);
}


void run_aas_callback(void *userdata, int flags) {
    AprilASRSession session = userdata;

    if(flags & PT_FLAG_FLUSH) {
        _aas_flush(session);
    }

    if(flags & PT_FLAG_AUDIO) {
        for(;;){
            size_t short_count = 3200;
            short *shorts = ap_pull_audio(session->provider, &short_count);
            if(short_count == 0) return;

            _aas_feed_pcm16(session, shorts, short_count);

            ap_pull_audio_finish(session->provider, short_count);
        }
    }
}
