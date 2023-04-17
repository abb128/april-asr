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

#include <time.h>
#include "common.h"
#include "log.h"
#include "params.h"
#include "april_session.h"

void run_aas_callback(void *userdata, int flags);

AprilASRSession aas_create_session(AprilASRModel model, AprilConfig config) {
    AprilASRSession aas = (AprilASRSession)calloc(1, sizeof(struct AprilASRSession_i));

    aas->sync = ((config.flags & APRIL_CONFIG_FLAG_ASYNC_RT_BIT) | (config.flags & APRIL_CONFIG_FLAG_ASYNC_NO_RT_BIT)) == 0;
    aas->force_realtime = (config.flags & APRIL_CONFIG_FLAG_ASYNC_RT_BIT) != 0;

    FBankOptions fbank_opts = model->fbank_opts;
    fbank_opts.use_sonic = aas->force_realtime;

    aas->model = model;
    aas->fbank = make_fbank(fbank_opts);

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

    aas->emitted_silence = true;
    aas->was_flushed = false;

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
    aas->speed_needed = 1.0;

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

float aas_realtime_get_speedup(AprilASRSession session) {
    return session->force_realtime ? (float)session->speed_needed : 1.0f;
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
    if(aas->active_token_head == 0) return;

    aas->handler(
        aas->userdata,
        APRIL_RESULT_RECOGNITION_FINAL,
        aas->active_token_head,
        aas->active_tokens
    );

    aas->last_handler_call_head = aas->active_token_head;
    aas->active_token_head = 0;
}

void aas_finalize_previous_words(AprilASRSession aas, const AprilToken *new_token){
    if(aas->active_token_head == 0) return;

    if(new_token->flags & APRIL_TOKEN_FLAG_WORD_BOUNDARY_BIT) {
        // The new token starts a new word, so we can break here
        return aas_finalize_tokens(aas);
    }else{
        // The new token continues the existing word. We need to move
        // the entire word into the next active_tokens

        // Find the start of the existing word
        size_t start_of_word = MAX_ACTIVE_TOKENS;
        for(size_t i=aas->active_token_head - 1; i > 2; i--){
            if(aas->active_tokens[i].flags & APRIL_TOKEN_FLAG_WORD_BOUNDARY_BIT) {
                start_of_word = i;
                break;
            }
        }

        if(start_of_word == MAX_ACTIVE_TOKENS) {
            // Couldn't find start of word, no good way to do this
            return aas_finalize_tokens(aas);
        }

        // Call FINAL excluding the current word
        aas->handler(
            aas->userdata,
            APRIL_RESULT_RECOGNITION_FINAL,
            start_of_word,
            aas->active_tokens
        );

        // Move the current word to start of tokens
        memmove(
            aas->active_tokens,
            &aas->active_tokens[start_of_word],
            sizeof(AprilToken) * (aas->active_token_head - start_of_word) // is this right? or off by one?
        );

        // Update active_token_head
        aas->active_token_head -= start_of_word;
    }
}

void aas_emit_silence(AprilASRSession aas) {
    if(!aas->emitted_silence){
        aas->emitted_silence = true;

        aas->handler(
            aas->userdata,
            APRIL_RESULT_SILENCE,
            0,
            NULL
        );
    }
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

void aas_clear_context(AprilASRSession aas) {
    if(aas->context.data[0] == aas->model->params.blank_id) return;

    for(int i=0; i<aas->context_size; i++)
        aas_update_context(aas, aas->model->params.blank_id);
}

// Processes current data in aas->logits. Returns false if new token was
// added, else returns true if no new data is available. Updates
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

    float blank_val = logits[blank];
    bool is_blank = (blank_val - early_emit) > max_val;


    AprilToken token = { get_token(params, max_idx), max_val };
    token.time_ms = aas->current_time_ms;

    // works for English and other latin languages, may need to do something
    // different here for other languages like Chinese
    if (token.token[0] == ' ') token.flags |= APRIL_TOKEN_FLAG_WORD_BOUNDARY_BIT;

    // Be more liberal with applying punctuation (might be just a model issue)
    bool is_punct = (token.token[0] == '.') || (token.token[0] == ',') || (token.token[0] == '!') || (token.token[0] == '?');
    if(is_punct && (!is_equal_to_previous) && (max_val > (blank_val - 3.5f))) {
        if(aas->active_token_head > 0) {
            const char *last_token = aas->active_tokens[aas->active_token_head - 1].token;
            if((last_token[0] >= '0') && (last_token[0] <= '9') && (token.token[0] == '.')){
                // Skip when the . is inside a number
            }else{
                is_blank = false;
            }
        }
    }

    // If current token is non-blank, emit and return
    if(!is_blank) {
        aas->last_emission_time_ms = aas->current_time_ms;

        aas_update_context(aas, (int64_t)max_idx);

        bool is_final = (aas->active_token_head >= (MAX_ACTIVE_TOKENS - 1));

        // Force final if a new sentence has started
        if(aas->active_token_head > 0) {
            const char *last_token = aas->active_tokens[aas->active_token_head - 1].token;
            if((last_token[1] == 0) && ((last_token[0] == '.') || (last_token[0] == '!') || (last_token[0] == '?'))){
                if((token.flags & APRIL_TOKEN_FLAG_WORD_BOUNDARY_BIT) != 0){
                    is_final = true;
                }
            }
        }

        if(is_final) aas_finalize_previous_words(aas, &token);

        is_final = (aas->active_token_head >= (MAX_ACTIVE_TOKENS - 1));
        if(is_final) {
            LOG_ERROR("No room left even after finalizing previous words");
            aas->active_token_head = 0;
        }

        aas_emit_token(aas, &token, true);

        aas->emitted_silence = false;
    } else {
        size_t time_since_emission_ms = aas->current_time_ms - aas->last_emission_time_ms;

        // If there's been silence for a while, forcibly reduce confidence to
        // kill stray prediction
        max_val -= (float)(time_since_emission_ms)/3000.0f;

        // If current token is blank, but it's reasonably confident, emit
        bool reasonably_confident = (!is_equal_to_previous) && (max_val > (blank_val - 4.0f));

        bool been_long_silence = time_since_emission_ms >= 2200;

        if (been_long_silence) {
            aas_finalize_tokens(aas);
            aas_clear_context(aas);
            aas_emit_silence(aas);
        } else if(reasonably_confident) {
            token.logprob -= 8.0;
            if(aas_emit_token(aas, &token, false)) {
                assert(aas->active_token_head > 0);
                aas->active_token_head--;
            }
        } else {
            aas_emit_token(aas, NULL, false);
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
        size_t stride_ms = fbank_get_segments_stride_ms(aas->fbank);
        aas->current_time_ms += stride_ms;

        clock_t clock_start = clock();

        aas_run_encoder(aas);

        float early_emit = 2.0f;
        for(int i=0; i<3; i++){
            early_emit -= 1.0f;
            aas_run_joiner(aas);
            if(aas_process_logits(aas, early_emit > 0.0f ? early_emit : 0.0f)) break;
        }

        clock_t clock_end = clock();

        double time_used_ms = ((double)(clock_end - clock_start) * 1000.0) / ((double)CLOCKS_PER_SEC);
        double stride_ms_d = (double)stride_ms;

        double speed_needed = (time_used_ms * 1.1) / stride_ms_d;
        aas->speed_needed = ((aas->speed_needed * 9.0) + speed_needed)/10.0;

        aas->time_since_update_speed += stride_ms;

        any_inferred = true;
    }

    if(aas->force_realtime && (aas->time_since_update_speed > 2000)) {
        fbank_set_speed(aas->fbank, aas->speed_needed > 1.0 ? aas->speed_needed : 1.0);

        aas->time_since_update_speed = 0;
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

void _aas_flush(AprilASRSession session) {
    if(session->was_flushed) return;

    session->was_flushed = true;

    while(fbank_flush(session->fbank))
        aas_infer(session);

    for(int i=0; i<2; i++)
        fbank_accept_waveform(session->fbank, NULL, SEGSIZE);

    while(fbank_flush(session->fbank))
        aas_infer(session);

    aas_finalize_tokens(session);
    aas_clear_context(session);
    aas_emit_silence(session);
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
