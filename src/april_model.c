#include "file/model_file.h"
#include "april_model.h"

AprilASRModel aam_create_model(const char *model_path) {
    ModelFile file = model_read(model_path);
    if(file == NULL) {
        printf("aam: failed to read file\n");
        return NULL;
    }

    assert(model_type(file) == MODEL_LSTM_TRANSDUCER_STATELESS);
    assert(model_network_count(file) == 3);

    printf("aam: using model %s\n", model_name(file));


    AprilASRModel aam = (AprilASRModel)calloc(1, sizeof(struct AprilASRModel_i));
    
    ORT_ABORT_ON_ERROR(g_ort->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "aam", &aam->env));
    assert(aam->env != NULL);

    ORT_ABORT_ON_ERROR(g_ort->CreateSessionOptions(&aam->session_options));
    ORT_ABORT_ON_ERROR(g_ort->SetIntraOpNumThreads(aam->session_options, 1));
    ORT_ABORT_ON_ERROR(g_ort->SetInterOpNumThreads(aam->session_options, 1));

    load_network_from_model_file(aam->env, aam->session_options, file, 0, &aam->encoder);
    load_network_from_model_file(aam->env, aam->session_options, file, 1, &aam->decoder);
    load_network_from_model_file(aam->env, aam->session_options, file, 2, &aam->joiner);

    model_read_params(file, &aam->params);

    free_model(file);
    

    assert(input_count(aam->encoder)  == 3);
    assert(output_count(aam->encoder) == 3);

    assert(input_count(aam->decoder) == 1);
    assert(output_count(aam->decoder) == 1);
    
    assert(input_count(aam->joiner)  == 2);
    assert(output_count(aam->joiner) == 1);

    assert(input_dims(aam->encoder, 0, aam->x_dim, 3) == 3);
    assert(input_dims(aam->encoder, 1, aam->h_dim, 3) == 3);
    assert(input_dims(aam->encoder, 2, aam->c_dim, 3) == 3);
    assert(output_dims(aam->encoder, 0, aam->eout_dim, 3) == 3);

    assert(input_dims(aam->decoder, 0, aam->context_dim, 2) == 2);
    assert(output_dims(aam->decoder, 0, aam->dout_dim, 3) == 3);

    assert(output_dims(aam->joiner, 0, aam->logits_dim, 3) == 3);

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