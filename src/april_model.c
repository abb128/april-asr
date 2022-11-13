#include "april_model.h"

AprilASRModel aam_create_model(const char *model_dir) {
    AprilASRModel aam = (AprilASRModel)calloc(1, sizeof(struct AprilASRModel_i));
    
    ORT_ABORT_ON_ERROR(g_ort->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "test", &aam->env));
    assert(aam->env != NULL);

    ORT_ABORT_ON_ERROR(g_ort->CreateSessionOptions(&aam->session_options));
    ORT_ABORT_ON_ERROR(g_ort->SetIntraOpNumThreads(aam->session_options, 1));
    ORT_ABORT_ON_ERROR(g_ort->SetInterOpNumThreads(aam->session_options, 1));

    // TODO later later: maybe combine everything into one nice big file
    ORTCHAR_T model_path[1024];
    
    SET_CONCAT_PATH(model_path, model_dir, "encoder.onnx");
    ORT_ABORT_ON_ERROR(g_ort->CreateSession(aam->env, model_path, aam->session_options, &aam->encoder));
    
    SET_CONCAT_PATH(model_path, model_dir, "joiner.onnx");
    ORT_ABORT_ON_ERROR(g_ort->CreateSession(aam->env, model_path, aam->session_options, &aam->joiner));

    SET_CONCAT_PATH(model_path, model_dir, "params.bin");
    read_params(&aam->params, model_path);

    assert(input_count(aam->encoder)  == 3);
    assert(output_count(aam->encoder) == 3);
    
    assert(input_count(aam->joiner)  == 2);
    assert(output_count(aam->joiner) == 1);

    assert(input_dims(aam->encoder, 0, aam->x_dim, 3) == 3);
    assert(input_dims(aam->encoder, 1, aam->h_dim, 3) == 3);
    assert(input_dims(aam->encoder, 2, aam->c_dim, 3) == 3);
    assert(output_dims(aam->encoder, 0, aam->eout_dim, 3) == 3);

    assert(input_dims(aam->joiner, 0, aam->context_dim, 2) == 2);
    assert(output_dims(aam->joiner, 0, aam->logits_dim, 3) == 3);

    aam->fbank_opts.sample_freq = aam->params.sample_rate;
    aam->fbank_opts.num_bins    = aam->params.mel_features;
    aam->fbank_opts.pull_segment_count = aam->params.segment_size;
    aam->fbank_opts.pull_segment_step  = aam->params.segment_step;

    // TODO: read these from config file
    aam->fbank_opts.frame_shift_ms = 10;
    aam->fbank_opts.frame_length_ms = 25;
    aam->fbank_opts.round_pow2 = true;
    aam->fbank_opts.mel_low = 20;
    aam->fbank_opts.mel_high = 0;
    aam->fbank_opts.snip_edges = true;

    assert(aam->x_dim[0] == aam->params.batch_size);
    assert(aam->x_dim[1] == aam->fbank_opts.pull_segment_count);
    assert(aam->x_dim[2] == aam->fbank_opts.num_bins);
    assert(aam->logits_dim[2] == aam->params.token_count);

    return aam;
}

void aam_free(AprilASRModel model) {
    free_params(&model->params);

    g_ort->ReleaseSession(model->joiner);
    g_ort->ReleaseSession(model->encoder);
    g_ort->ReleaseSessionOptions(model->session_options);
    g_ort->ReleaseEnv(model->env);

    free(model);
}