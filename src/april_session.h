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

#ifndef _APRIL_SESSION
#define _APRIL_SESSION

#include "common.h"
#include <stdbool.h>
#include "april_model.h"
#include "april_api.h"
#include "fbank.h"

#include "model_impl.h"
#include "audio_provider.h"
#include "proc_thread.h"

#define MAX_ACTIVE_TOKENS 72

struct AprilASRSession_i {
    AprilASRModel model;
    OnlineFBank fbank;

    AprilToken active_tokens[MAX_ACTIVE_TOKENS];
    size_t active_token_head;

    struct april_model_tensors tensors;

    size_t context_size;

    bool emitted_silence;
    bool was_flushed;

    bool sync;
    bool force_realtime;

    AudioProvider provider;

    size_t current_time_ms;
    size_t last_emission_time_ms;
    size_t last_handler_call_head;

    // set by user
    AprilRecognitionResultHandler handler;
    void *userdata;
};

int aas_run_self_pt_tasks(AprilASRSession sess);
bool aas_process_logits(AprilASRSession aas, float early_emit);

#endif