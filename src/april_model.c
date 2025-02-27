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

#include "common.h"
#include "file/model_file.h"
#include "april_model.h"
#include "log.h"

#define ASSERT_OR_RETURN_NULL(expr) if(!(expr)) { LOG_WARNING("Model: assertion " #expr " failed, line %d", __LINE__); return NULL; }
#define ASSERT_OR_FREE_AAM_AND_RETURN_NULL(aam, expr) if(!(expr)) { LOG_WARNING("Model: assertion " #expr " failed, line %d", __LINE__); aam_free(aam); return NULL; }
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
    
    ORT_ABORT_ON_ERROR(g_ort->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "aam", &aam->env));
    if(aam->env == NULL) {
        LOG_ERROR("Creating ORT environment failed!");
        free_model(file);
        aam_free(aam);
        return NULL;
    }

    ORT_ABORT_ON_ERROR(g_ort->CreateSessionOptions(&aam->session_options));
    ORT_ABORT_ON_ERROR(g_ort->SetIntraOpNumThreads(aam->session_options, 1));
    ORT_ABORT_ON_ERROR(g_ort->SetInterOpNumThreads(aam->session_options, 1));

    load_network_from_model_file(aam->env, aam->session_options, file, 0, &aam->encoder);
    load_network_from_model_file(aam->env, aam->session_options, file, 1, &aam->decoder);
    load_network_from_model_file(aam->env, aam->session_options, file, 2, &aam->joiner);

    model_read_params(file, &aam->params);

    transfer_strings_and_free_model(file, &aam->name, &aam->description, &aam->language);

    ASSERT_OR_FREE_AAM_AND_RETURN_NULL(aam, input_count(aam->encoder)  == 3);
    ASSERT_OR_FREE_AAM_AND_RETURN_NULL(aam, output_count(aam->encoder) == 3);

    ASSERT_OR_FREE_AAM_AND_RETURN_NULL(aam, input_count(aam->decoder) == 1);
    ASSERT_OR_FREE_AAM_AND_RETURN_NULL(aam, output_count(aam->decoder) == 1);
    
    ASSERT_OR_FREE_AAM_AND_RETURN_NULL(aam, input_count(aam->joiner)  == 2);
    ASSERT_OR_FREE_AAM_AND_RETURN_NULL(aam, output_count(aam->joiner) == 1);

    input_dims(aam->encoder, 0, aam->x_dim, 3);
    input_dims(aam->encoder, 1, aam->h_dim, 3);
    input_dims(aam->encoder, 2, aam->c_dim, 3);
    output_dims(aam->encoder, 0, aam->eout_dim, 3);

    input_dims(aam->decoder, 0, aam->context_dim, 2);
    output_dims(aam->decoder, 0, aam->dout_dim, 3);

    output_dims(aam->joiner, 0, aam->logits_dim, 3);

    aam->fbank_opts.sample_freq        = aam->params.sample_rate;
    aam->fbank_opts.num_bins           = aam->params.mel_features;
    aam->fbank_opts.pull_segment_count = aam->params.segment_size;
    aam->fbank_opts.pull_segment_step  = aam->params.segment_step;
    aam->fbank_opts.frame_shift_ms     = aam->params.frame_shift_ms;
    aam->fbank_opts.frame_length_ms    = aam->params.frame_length_ms;
    aam->fbank_opts.round_pow2         = aam->params.round_pow2;
    aam->fbank_opts.mel_low            = aam->params.mel_low;
    aam->fbank_opts.mel_high           = aam->params.mel_high;
    //aam->fbank_opts.snip_edges         = aam->params.snip_edges;
    aam->fbank_opts.snip_edges = true;

    aam->fbank_opts.remove_dc_offset = true;
    aam->fbank_opts.preemph_coeff = 0.97f;

    ASSERT_OR_FREE_AAM_AND_RETURN_NULL(aam, aam->x_dim[0] == aam->params.batch_size);
    ASSERT_OR_FREE_AAM_AND_RETURN_NULL(aam, aam->x_dim[1] == aam->fbank_opts.pull_segment_count);
    ASSERT_OR_FREE_AAM_AND_RETURN_NULL(aam, aam->x_dim[2] == aam->fbank_opts.num_bins);
    ASSERT_OR_FREE_AAM_AND_RETURN_NULL(aam, aam->logits_dim[2] == aam->params.token_count);

    LOG_INFO("aam: loaded model %s", aam->name);

    return aam;
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
