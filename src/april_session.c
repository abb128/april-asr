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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
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
    fbank_opts.use_sonic = false;//aas->force_realtime; // TODO: Implement

    aas->model = model;
    aas->fbank = make_fbank(fbank_opts);

    aas->active_token_head = 0;
    aas->context_size = 2;

    aas->emitted_silence = true;
    aas->was_flushed = false;

    aas->handler = config.handler;
    aas->userdata = config.userdata;

    if(aas->handler == NULL) {
        LOG_ERROR("No handler provided! A handler is required, please provide a handler");
        aas_free(aas);
        return NULL;
    }

    aas->provider = ap_create();

    model_alloc_tensors(model->model, &aas->tensors);

    // Insert into sessions. TODO: Handle having too many sessions
    for(int i=0; i<MAX_SESSIONS; i++) {
        if(model->sessions[i] == NULL) {
            model->sessions[i] = aas;
            break;
        }
    }

    aas->tensors.token_ctx[0] = 0;
    aas->tensors.token_ctx[1] = 0;
    aas->tensors.requires_decoding = true;

    return aas;
}

float aas_realtime_get_speedup(AprilASRSession session) {
    return 1.0f;// TODO: session->force_realtime ? (float)session->speed_needed : 1.0f;
}

void aas_free(AprilASRSession session) {
    if(session == NULL) return;

    // TODO: Free tensors (they are being leaked right now)

    for(int i=0; i<MAX_SESSIONS; i++) {
        if(session->model->sessions[i] == session) {
            session->model->sessions[i] = NULL;
            break;
        }
    }

    ap_free(session->provider);
    free_fbank(session->fbank);
    free(session);
}

void aas_update_context(AprilASRSession aas, int64_t new_token){
    assert(aas->context_size == 2);
    aas->tensors.token_ctx[0] = aas->tensors.token_ctx[1];
    aas->tensors.token_ctx[1] = new_token;
    aas->tensors.requires_decoding = true;
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
    if(aas->tensors.token_ctx[0] == aas->model->params.blank_id) return;

    for(int i=0; i<aas->context_size; i++)
        aas_update_context(aas, aas->model->params.blank_id);
}

// Processes current data in aas->logits. Returns false if new token was
// added, else returns true if no new data is available. Updates
// aas->context and aas->active_tokens. Uses basic greedy search algorithm.
bool aas_process_logits(AprilASRSession aas, float early_emit){
    ModelParameters *params = &aas->model->params;
    size_t blank = params->blank_id;
    float *logits = aas->tensors.logits;

    int max_idx = -1;
    float max_val = -9999999999.0;
    for(size_t i=0; i<(size_t)params->token_count; i++){
        if(i == blank) continue;

        if(logits[i] > max_val){
            max_idx = i;
            max_val = logits[i];
        }
    }

    bool was_context_cleared = aas->tensors.token_ctx[1] == aas->model->params.blank_id;

    // If the current token is equal to previous, ignore early_emit.
    // Helps prevent repeating like ALUMUMUMUMUMUININININIUM which happens for some reason
    bool is_equal_to_previous = aas->tensors.token_ctx[1] == max_idx;
    if(is_equal_to_previous) early_emit = 0.0f;

    float blank_val = logits[blank];
    bool is_blank = (blank_val - early_emit) > max_val;


    AprilToken token = { get_token(params, max_idx), max_val };
    token.time_ms = aas->current_time_ms;

    // works for English and other latin languages, may need to do something
    // different here for other languages like Chinese
    if (token.token[0] == ' ') token.flags |= APRIL_TOKEN_FLAG_WORD_BOUNDARY_BIT;

    bool is_single_character = token.token[1] == 0;
    bool is_end_of_sentence = is_single_character && ( (token.token[0] == '.') || (token.token[0] == '!') || (token.token[0] == '?') );
    bool is_punctuation = is_end_of_sentence || (is_single_character && (token.token[0] == ','));

    // Don't treat the "." in a number (like 10.0) as punctuation or end of sentence
    if(is_punctuation && (aas->active_token_head > 0)){
        const char *last_token = aas->active_tokens[aas->active_token_head - 1].token;
        if((last_token[0] >= '0') && (last_token[0] <= '9') && (token.token[0] == '.')){
            is_end_of_sentence = false;
            is_punctuation = false;
        }
    }
    
    if(is_end_of_sentence) token.flags |= APRIL_TOKEN_FLAG_SENTENCE_END_BIT;

    // Be more liberal with applying punctuation (might be just a model issue)
    if((!was_context_cleared) && is_punctuation && (!is_equal_to_previous) && (max_val > (blank_val - 3.5f))) {
        is_blank = false;
    }

    // If current token is non-blank, emit and return
    if(!is_blank) {
        aas->last_emission_time_ms = aas->current_time_ms;

        aas_update_context(aas, (int64_t)max_idx);

        bool is_final = (aas->active_token_head >= (MAX_ACTIVE_TOKENS - 1));

        // Sentence boundary checks
        if((aas->active_token_head > 0) && ((token.flags & APRIL_TOKEN_FLAG_WORD_BOUNDARY_BIT) != 0)) {
            const char *last_token = aas->active_tokens[aas->active_token_head - 1].token;
            int last_token_flags = aas->active_tokens[aas->active_token_head - 1].flags;

            bool last_token_single_character = last_token[1] == 0;
            bool last_token_end_of_sentence = last_token_single_character && ( (last_token[0] == '.') || (last_token[0] == '!') || (last_token[0] == '?') );

            // If this token is a word boundary, and the last character was supposed to be
            // end of sentence, but wasn't treated as such because it came after a
            // number, mark it as end of sentence (so "the number is 3. hi there"
            // has the correct end of sentence token
            if(last_token_end_of_sentence && ((last_token_flags & APRIL_TOKEN_FLAG_SENTENCE_END_BIT) == 0)){
                aas->active_tokens[aas->active_token_head - 1].flags |= APRIL_TOKEN_FLAG_SENTENCE_END_BIT;
            }
            
            // Force final if a new sentence has started
            if(last_token_end_of_sentence){
                is_final = true;
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
    if(!aas->sync) return false;

    // In synchronous session, call collect_loop with our session
    // This will perform inference with our session only (batch size 1)
    return aam_collect_loop(aas->model, aas) > 0;
}

void _aas_feed_pcm16(AprilASRSession session, short *pcm16, size_t short_count);
void aas_feed_pcm16(AprilASRSession session, short *pcm16, size_t short_count) {
    if(session->sync) return _aas_feed_pcm16(session, pcm16, short_count);

    bool success = ap_push_audio(session->provider, pcm16, short_count);

    aam_pt_raise_flag(session->model);

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
static FILE *fd = NULL;
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

        if(session->sync) aas_infer(session);

        head += remaining;
    }

#ifdef APRIL_DEBUG_SAVE_AUDIO
    fflush(fd);
#endif
}

void _aas_flush(AprilASRSession session);
void aas_flush(AprilASRSession session) {
    if(session->sync) return _aas_flush(session);

    // TODO: Implement
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

int aas_run_self_pt_tasks(AprilASRSession aas) {
    // TODO: Check if aas->flush_flag then flush

    // TODO: This only pulls once, if the caller is calling with
    // larger amount of data we might fall behind
    size_t short_count = 3200;
    short *shorts = ap_pull_audio(aas->provider, &short_count);
    if(short_count == 0) return 0;

    _aas_feed_pcm16(aas, shorts, short_count);

    ap_pull_audio_finish(aas->provider, short_count);
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
