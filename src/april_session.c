#include "params.h"
#include "april_session.h"

AprilASRSession aas_create_session(
    AprilASRModel model,
    AprilRecognitionResultHandler handler,
    void *userdata,
    AprilSpeakerID *id
) {
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

    aas->context = alloc_tensor2i(mi, model->context_dim);
    for(int i=0; i<SHAPE_PRODUCT2(model->context_dim); i++) aas->context.data[i] = 0;

    aas->logits = alloc_tensor3f(mi, model->logits_dim);

    aas->hc_use_0 = false;
    aas->active_token_head = 0;

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

    aas->handler = handler;
    aas->userdata = userdata;
    assert(handler != NULL);

    return aas;
}

void aas_free(AprilASRSession session) {
    free_tensorf(&session->logits);
    free_tensori(&session->context);
    free_tensorf(&session->eout);
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

const char* joiner_input_names[] = {"context", "encoder_out"};
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

// Runs joiner on current data in aas->context and aas->eout
void aas_run_joiner(AprilASRSession aas){
    const OrtValue *inputs[] = {
        aas->context.tensor,
        aas->eout.tensor
    };
    
    OrtValue *outputs[] = {
        aas->logits.tensor
    };

    ORT_ABORT_ON_ERROR(g_ort->Run(aas->model->joiner, NULL,
                                    joiner_input_names, inputs, 2,
                                    joiner_output_names, 1, outputs));
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
    for(int i=0; i<params->token_count; i++){
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

    // If current token is non-blank, emit and return
    if(!is_blank) {
        aas->runs_since_emission = 0;

        aas->context.data[0] = aas->context.data[1];
        aas->context.data[1] = (int64_t)max_idx;

        aas->handler(aas->userdata, APRIL_RESULT_RECOGNITION_FINAL, 1, &token);
    } else {
        aas->runs_since_emission += 1;

        // If there's been silence for a while, forcibly reduce confidence to
        // kill stray prediction
        max_val -= (float)(aas->runs_since_emission-1)/10.0f;

        // If current token is blank, but it's reasonably confident, emit as partial
        if(!is_equal_to_previous && (max_val > (blank_val - 4.0f))) {
            aas->handler(aas->userdata, APRIL_RESULT_RECOGNITION_PARTIAL, 1, &token);
        } else {
            // Emit blank partial
            const char *blank = "\0\0";
            aas->handler(aas->userdata, APRIL_RESULT_RECOGNITION_PARTIAL, 0, NULL);
        }
    }

    return is_blank;
}

bool aas_infer(AprilASRSession aas){
    bool any_inferred = false;
    while(fbank_pull_segments( aas->fbank, aas->x.data, sizeof(float)*SHAPE_PRODUCT3(aas->model->x_dim) )){
        aas_run_encoder(aas);

        float early_emit = 3.0f;
        for(int i=0; i<8; i++){
            early_emit -= 1.0f;
            aas_run_joiner(aas);
            if(!aas_process_logits(aas, early_emit > 0.0f ? early_emit : 0.0f)) break;
        }
        
        any_inferred = true;
    }

    return any_inferred;
}

#define SEGSIZE 3200 //TODO
void aas_feed_pcm16(AprilASRSession session, short *pcm16, size_t short_count) {
    assert(session->fbank != NULL);
    assert(session != NULL);
    assert(pcm16 != NULL);

    size_t head = 0;
    float wave[SEGSIZE];

    while(head < short_count){
        size_t remaining = short_count - head;
        if(remaining < 1) break;
        if(remaining > SEGSIZE) remaining = SEGSIZE;

        for(int i=0; i<remaining; i++){
            wave[i] = (float)pcm16[head + i] / 32768.0f;
        }
        
        fbank_accept_waveform(session->fbank, wave, remaining);

        aas_infer(session);

        head += remaining;
    }
}


void aas_flush(AprilASRSession session) {
    while(fbank_flush(session->fbank))
        aas_infer(session);
}