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

#ifndef _APRIL_MODEL
#define _APRIL_MODEL

#include "common.h"
#include "april_api.h"
#include "params.h"
#include "fbank.h"
#include "model_impl.h"
#include "proc_thread.h"

#define MAX_SESSIONS 64
struct AprilASRModel_i {
    struct april_model_ggml* model;
    struct april_context_ggml *ctx;

    AprilASRSession sessions[MAX_SESSIONS];

    FBankOptions fbank_opts;
    ModelParameters params;
    ProcThread proc_thread;
};

void aam_pt_raise_flag(AprilASRModel model);
void aam_ensure_proc_thread_spawned(AprilASRModel model);
int aam_collect_loop(AprilASRModel model, AprilASRSession force_session);
int aam_collect_and_encode(AprilASRModel model, AprilASRSession force_session);
int aam_collect_and_decode(AprilASRModel model, AprilASRSession force_session);
int aam_collect_and_join(AprilASRModel model, AprilASRSession force_session);
#endif