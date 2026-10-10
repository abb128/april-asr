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

#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE /* sched_getaffinity */
#endif
#include "common.h"
#include "file/model_file.h"
#include "april_model.h"
#include "log.h"
#include "settings.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif
#ifdef __linux__
#include <sched.h>
#endif

/* CPUs this process may run on, honouring affinity (taskset, cpusets). */
static long online_cpu_count(void) {
#ifdef _WIN32
    DWORD_PTR process_mask, system_mask;
    if (GetProcessAffinityMask(GetCurrentProcess(), &process_mask, &system_mask)) {
        long count = 0;
        for (; process_mask; process_mask &= process_mask - 1) count++;
        if (count > 0) return count;
    }
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return (long)info.dwNumberOfProcessors;
#else
#ifdef __linux__
    cpu_set_t set;
    if (sched_getaffinity(0, sizeof(set), &set) == 0 && CPU_COUNT(&set) > 0)
        return CPU_COUNT(&set);
#endif
    return sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

#define MODEL_REQUIRE(expr) do { if (!(expr)) { \
    LOG_WARNING("Model validation failed: %s", #expr); goto fail; \
} } while (0)
#define MODEL_ORT(expr) MODEL_REQUIRE(ort_ok(expr))

AprilASRModel aam_create_model(const char *model_path) {
    if(g_ort == NULL) {
        LOG_ERROR("aam: g_ort is NULL, please make sure to call aam_api_init!");
        return NULL;
    }
    
    ModelFile file = model_read(model_path);
    if(file == NULL) {
        LOG_ERROR("aam: failed to read file");
        return NULL;
    }

    if((model_type(file) != MODEL_LSTM_TRANSDUCER_STATELESS) || (model_network_count(file) != 3)) {
        LOG_WARNING("Model has unknown model type, or the wrong number of networks");
        free_model(file);
        return NULL;
    }


    AprilASRModel aam = (AprilASRModel)calloc(1, sizeof(struct AprilASRModel_i));
    if (!aam) { free_model(file); return NULL; }
    
    /* Reject corrupt parameters before loading hundreds of megabytes of ONNX. */
    MODEL_REQUIRE(model_read_params(file, &aam->params));

    MODEL_ORT(g_ort->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "aam", &aam->env));
    /* Local captioning must not opt into runtime telemetry. Apply the ORT
       opt-out before loading networks or feeding any user audio. */
    MODEL_ORT(g_ort->DisableTelemetryEvents(aam->env));
    if(aam->env == NULL) {
        LOG_ERROR("Creating ORT environment failed!");
        free_model(file);
        aam_free(aam);
        return NULL;
    }

    MODEL_ORT(g_ort->CreateSessionOptions(&aam->session_options));
    MODEL_ORT(g_ort->SetIntraOpNumThreads(aam->session_options, 1));
    MODEL_ORT(g_ort->SetInterOpNumThreads(aam->session_options, 1));
    MODEL_ORT(g_ort->SetSessionExecutionMode(aam->session_options, ORT_SEQUENTIAL));
    MODEL_ORT(g_ort->SetSessionGraphOptimizationLevel(aam->session_options, ORT_ENABLE_ALL));
    const char *spinning = april_env_int("APRIL_SPINNING", 0, 0, 1) ? "1" : "0";
    MODEL_ORT(g_ort->AddSessionConfigEntry(aam->session_options, "session.intra_op.allow_spinning", spinning));
    MODEL_ORT(g_ort->AddSessionConfigEntry(aam->session_options, "session.inter_op.allow_spinning", spinning));

    /* Only the larger encoder gets a configurable pool. The small decoder
       and joiner stay single-threaded, avoiding three competing pools.
       Two threads give most of the speedup on typical desktop CPUs; more
       add little for 40 ms steps and take cores from the rest of the system. */
    int default_threads = online_cpu_count() >= 2 ? 2 : 1;
    int encoder_threads = april_env_int("APRIL_ENCODER_THREADS", default_threads, 1, 64);
    MODEL_ORT(g_ort->SetIntraOpNumThreads(aam->session_options, encoder_threads));

    MODEL_REQUIRE(load_network_from_model_file(aam->env, aam->session_options, file, 0, &aam->encoder));
    MODEL_ORT(g_ort->SetIntraOpNumThreads(aam->session_options, 1));
    MODEL_REQUIRE(load_network_from_model_file(aam->env, aam->session_options, file, 1, &aam->decoder));
    MODEL_REQUIRE(load_network_from_model_file(aam->env, aam->session_options, file, 2, &aam->joiner));

    MODEL_REQUIRE(input_count(aam->encoder) == 3 && output_count(aam->encoder) == 3);
    MODEL_REQUIRE(input_count(aam->decoder) == 1 && output_count(aam->decoder) == 1);
    MODEL_REQUIRE(input_count(aam->joiner) == 2 && output_count(aam->joiner) == 1);
    const ONNXTensorElementDataType f32 = ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT;
    MODEL_REQUIRE(tensor_info(aam->encoder, false, 0, "x", f32, aam->x_dim, 3));
    MODEL_REQUIRE(tensor_info(aam->encoder, false, 1, "h", f32, aam->h_dim, 3));
    MODEL_REQUIRE(tensor_info(aam->encoder, false, 2, "c", f32, aam->c_dim, 3));
    MODEL_REQUIRE(tensor_info(aam->encoder, true, 0, "encoder_out", f32, aam->eout_dim, 3));
    MODEL_REQUIRE(tensor_info(aam->decoder, false, 0, "context", ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64, aam->context_dim, 2));
    MODEL_REQUIRE(tensor_info(aam->decoder, true, 0, "decoder_out", f32, aam->dout_dim, 3));
    MODEL_REQUIRE(tensor_info(aam->joiner, true, 0, "logits", f32, aam->logits_dim, 3));
    int64_t dims[3];
    MODEL_REQUIRE(tensor_info(aam->encoder, true, 1, "next_h", f32, dims, 3) && !memcmp(dims, aam->h_dim, sizeof(dims)));
    MODEL_REQUIRE(tensor_info(aam->encoder, true, 2, "next_c", f32, dims, 3) && !memcmp(dims, aam->c_dim, sizeof(dims)));
    MODEL_REQUIRE(tensor_info(aam->joiner, false, 0, "encoder_out", f32, dims, 3) && !memcmp(dims, aam->eout_dim, sizeof(dims)));
    MODEL_REQUIRE(tensor_info(aam->joiner, false, 1, "decoder_out", f32, dims, 3) && !memcmp(dims, aam->dout_dim, sizeof(dims)));

    aam->fbank_opts.sample_freq        = aam->params.sample_rate;
    aam->fbank_opts.num_bins           = aam->params.mel_features;
    aam->fbank_opts.pull_segment_count = aam->params.segment_size;
    aam->fbank_opts.pull_segment_step  = aam->params.segment_step;
    aam->fbank_opts.frame_shift_ms     = aam->params.frame_shift_ms;
    aam->fbank_opts.frame_length_ms    = aam->params.frame_length_ms;
    aam->fbank_opts.round_pow2         = aam->params.round_pow2;
    aam->fbank_opts.mel_low            = aam->params.mel_low;
    aam->fbank_opts.mel_high           = aam->params.mel_high;
    aam->fbank_opts.corrected_window = april_env_int("APRIL_CORRECT_FBANK", 1, 0, 1) != 0;
    aam->fbank_opts.snip_edges = aam->fbank_opts.corrected_window ? aam->params.snip_edges : true;

    aam->fbank_opts.remove_dc_offset = true;
    aam->fbank_opts.preemph_coeff = 0.97f;

    MODEL_REQUIRE(aam->x_dim[0] == aam->params.batch_size);
    MODEL_REQUIRE(aam->x_dim[1] == aam->fbank_opts.pull_segment_count);
    MODEL_REQUIRE(aam->x_dim[2] == aam->fbank_opts.num_bins);
    MODEL_REQUIRE(aam->logits_dim[2] == aam->params.token_count);
    MODEL_REQUIRE(aam->context_dim[0] == 1 && aam->context_dim[1] > 0);
    MODEL_REQUIRE(aam->eout_dim[0] == 1 && aam->eout_dim[1] == 1 && aam->eout_dim[2] > 0);
    MODEL_REQUIRE(aam->dout_dim[0] == 1 && aam->dout_dim[1] == 1 && aam->dout_dim[2] > 0);

    MODEL_REQUIRE(aam->h_dim[1] == 1 && aam->c_dim[1] == 1);
    MODEL_REQUIRE(aam->logits_dim[0] == 1 && aam->logits_dim[1] == 1);
    transfer_strings_and_free_model(file, &aam->name, &aam->description, &aam->language);
    file = NULL;
    MODEL_REQUIRE(aam->language);
    LOG_INFO("aam: loaded model %s", aam->name);

    return aam;
fail:
    free_model(file);
    aam_free(aam);
    return NULL;
}

const char *aam_get_name(AprilASRModel model) { return model->name; }
const char *aam_get_description(AprilASRModel model) { return model->description; }
const char *aam_get_language(AprilASRModel model) { return model->language; }

size_t aam_get_sample_rate(AprilASRModel model) {
    return model->fbank_opts.sample_freq;
}


void aam_free(AprilASRModel model) {
    if(model == NULL) return;

    free(model->name);
    free(model->description);
    free(model->language);

    free_params(&model->params);

    g_ort->ReleaseSession(model->joiner);
    g_ort->ReleaseSession(model->decoder);
    g_ort->ReleaseSession(model->encoder);
    g_ort->ReleaseSessionOptions(model->session_options);
    g_ort->ReleaseEnv(model->env);

    free(model);
}
