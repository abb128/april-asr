#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <stdbool.h>

#include "april.h"
#include "fbank.h"
#include "onnxruntime_c_api.h"

const OrtApi* g_ort = NULL;
#define ORT_ABORT_ON_ERROR(expr)                             \
  do {                                                       \
    OrtStatus* onnx_status = (expr);                         \
    if (onnx_status != NULL) {                               \
      const char* msg = g_ort->GetErrorMessage(onnx_status); \
      fprintf(stderr, "%s\n", msg);                          \
      g_ort->ReleaseStatus(onnx_status);                     \
      abort();                                               \
    }                                                        \
  } while (0);



void aam_api_init(void){
    g_ort = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    if (!g_ort) {
        fprintf(stderr, "Failed to init ONNX Runtime engine.\n");
        exit(-1);
    }
}

struct AprilASRModel_i {
    // Model components
    OrtEnv *env;
    OrtSessionOptions* session_options;


    OrtSession* s_encoder;
    OrtSession* s_decoder;
    OrtSession* s_joiner;
    OrtSession* s_joiner_decoder_proj;
    OrtSession* s_joiner_encoder_proj;

    int segment_size;

    // Inference components
    // ...
    OrtMemoryInfo* memory_info;

    OnlineFBank fbank;

    int64_t x_shape[3];
    float *x_data;

    int64_t x_len_shape[1];
    int64_t x_len_data[1];

    int64_t h_shape[3];
    float *h_0;
    float *h_1;

    int64_t c_shape[3];
    float *c_0;
    float *c_1;


    int64_t decoder_context_shape[2];
    int64_t *decoder_context;

    OrtValue *x_tensor;
    OrtValue *x_len_tensor;

    OrtValue *h_0_tensor;
    OrtValue *h_1_tensor;

    OrtValue *c_0_tensor;
    OrtValue *c_1_tensor;

    OrtValue *decoder_context_tensor;

    OrtValue *encoder_out3;
    OrtValue *encoder_out2;
    OrtValue *encoder_out_lens;
    OrtValue *decoder_out3;
    OrtValue *decoder_out2;
    OrtValue *projected_encoder_out;
    OrtValue *projected_decoder_out;

    OrtValue *logits;

    int active_tokens[64];
    int active_token_head;

    bool hc_swap;
};

#define SET_CONCAT_PATH(out_path, base, fname)          \
    do {                                                \
        memset(out_path, 0, sizeof(out_path));          \
        strcpy(out_path, base);                         \
        strcat(out_path, "/" fname);                    \
    } while (0)


#define SHAPE_PRODUCT1(shape) ((shape)[0])
#define SHAPE_PRODUCT2(shape) ((shape)[0] * (shape)[1])
#define SHAPE_PRODUCT3(shape) ((shape)[0] * (shape)[1] * (shape)[2])

AprilASRModel aam_create_model(const char *model_dir) {
    AprilASRModel aam = (AprilASRModel)calloc(1, sizeof(struct AprilASRModel_i));
    
    ORT_ABORT_ON_ERROR(g_ort->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "test", &aam->env));
    assert(aam->env != NULL);

    ORT_ABORT_ON_ERROR(g_ort->CreateSessionOptions(&aam->session_options));
    ORT_ABORT_ON_ERROR(g_ort->SetIntraOpNumThreads(aam->session_options, 1));
    ORT_ABORT_ON_ERROR(g_ort->SetInterOpNumThreads(aam->session_options, 1));

    // TODO later later: maybe combine everything into one nice big file
    ORTCHAR_T model_path[1024];
    
    
    printf("Loading models.\n");
    SET_CONCAT_PATH(model_path, model_dir, "encoder.onnx");
    ORT_ABORT_ON_ERROR(g_ort->CreateSession(aam->env, model_path, aam->session_options, &aam->s_encoder));
    
    SET_CONCAT_PATH(model_path, model_dir, "decoder.onnx");
    ORT_ABORT_ON_ERROR(g_ort->CreateSession(aam->env, model_path, aam->session_options, &aam->s_decoder));
    
    SET_CONCAT_PATH(model_path, model_dir, "joiner.onnx");
    ORT_ABORT_ON_ERROR(g_ort->CreateSession(aam->env, model_path, aam->session_options, &aam->s_joiner));
    
    SET_CONCAT_PATH(model_path, model_dir, "joiner_decoder_proj.onnx");
    ORT_ABORT_ON_ERROR(g_ort->CreateSession(aam->env, model_path, aam->session_options, &aam->s_joiner_decoder_proj));

    SET_CONCAT_PATH(model_path, model_dir, "joiner_encoder_proj.onnx");
    ORT_ABORT_ON_ERROR(g_ort->CreateSession(aam->env, model_path, aam->session_options, &aam->s_joiner_encoder_proj));
    printf("Loaded\n");

    aam->segment_size = 9;


    // TODO later: read this from a file
    FBankOptions opts = { 0 };
    opts.sample_freq = 16000;
    opts.frame_shift_ms = 10;
    opts.frame_length_ms = 25;
    opts.num_bins = 80;
    opts.round_pow2 = true;
    opts.mel_low = 20;
    opts.mel_high = 0;
    opts.snip_edges = true;
    opts.pull_segment_count = 9;
    opts.pull_segment_step = 4;

    aam->fbank = make_fbank(opts);



    aam->x_shape[0] = 1;
    aam->x_shape[1] = 9;
    aam->x_shape[2] = 80;
    aam->x_data = (float*)calloc(1*9*80, sizeof(float));

    aam->x_len_shape[0] = 1;
    aam->x_len_data[0] = 9;

    aam->h_shape[0] = 12;
    aam->h_shape[1] = 1;
    aam->h_shape[2] = 512;
    aam->h_0 = (float*)calloc(12*1*512, sizeof(float));
    aam->h_1 = (float*)calloc(12*1*512, sizeof(float));

    aam->c_shape[0] = 12;
    aam->c_shape[1] = 1;
    aam->c_shape[2] = 1024;
    aam->c_0 = (float*)calloc(12*1*1024, sizeof(float));
    aam->c_1 = (float*)calloc(12*1*1024, sizeof(float));


    const int context_size = 2; // TODO may differ
    aam->decoder_context_shape[0] = 1;
    aam->decoder_context_shape[1] = context_size;
    aam->decoder_context = (int64_t*)calloc(context_size, sizeof(int64_t));
    memset(aam->decoder_context, 0, context_size * sizeof(int64_t));


    aam->encoder_out3 = NULL;
    aam->encoder_out2 = NULL;
    aam->encoder_out_lens = NULL;
    aam->decoder_out3 = NULL;
    aam->decoder_out2 = NULL;
    aam->projected_decoder_out = NULL;
    aam->projected_encoder_out = NULL;
    aam->logits = NULL;
    aam->active_token_head = 0;



    OrtMemoryInfo* memory_info;
    ORT_ABORT_ON_ERROR(g_ort->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &aam->memory_info));


    // x tensor    
    ORT_ABORT_ON_ERROR(g_ort->CreateTensorWithDataAsOrtValue(aam->memory_info, 
                                                            aam->x_data, sizeof(float)*SHAPE_PRODUCT3(aam->x_shape),
                                                            aam->x_shape, 3,
                                                            ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
                                                            &aam->x_tensor));
    
    assert(aam->x_tensor != NULL);


    // x length tensor
    ORT_ABORT_ON_ERROR(g_ort->CreateTensorWithDataAsOrtValue(aam->memory_info, 
                                                            aam->x_len_data, sizeof(int64_t)*SHAPE_PRODUCT1(aam->x_len_shape),
                                                            aam->x_len_shape, 1,
                                                            ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64,
                                                            &aam->x_len_tensor));
    
    assert(aam->x_len_tensor != NULL);

    
    // h0, h1 tensors
    ORT_ABORT_ON_ERROR(g_ort->CreateTensorWithDataAsOrtValue(aam->memory_info, 
                                                            aam->h_0, sizeof(float)*SHAPE_PRODUCT3(aam->h_shape),
                                                            aam->h_shape, 3,
                                                            ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
                                                            &aam->h_0_tensor));
    ORT_ABORT_ON_ERROR(g_ort->CreateTensorWithDataAsOrtValue(aam->memory_info, 
                                                            aam->h_1, sizeof(float)*SHAPE_PRODUCT3(aam->h_shape),
                                                            aam->h_shape, 3,
                                                            ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
                                                            &aam->h_1_tensor));
    assert(aam->h_0_tensor != NULL);
    assert(aam->h_1_tensor != NULL);


    // c0, c1 tensors
    ORT_ABORT_ON_ERROR(g_ort->CreateTensorWithDataAsOrtValue(aam->memory_info, 
                                                            aam->c_0, sizeof(float)*SHAPE_PRODUCT3(aam->c_shape),
                                                            aam->c_shape, 3,
                                                            ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
                                                            &aam->c_0_tensor));
    ORT_ABORT_ON_ERROR(g_ort->CreateTensorWithDataAsOrtValue(aam->memory_info, 
                                                            aam->c_1, sizeof(float)*SHAPE_PRODUCT3(aam->c_shape),
                                                            aam->c_shape, 3,
                                                            ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
                                                            &aam->c_1_tensor));
    assert(aam->c_0_tensor != NULL);
    assert(aam->c_1_tensor != NULL);



    // decoder length tensor
    ORT_ABORT_ON_ERROR(g_ort->CreateTensorWithDataAsOrtValue(aam->memory_info, 
                                                            aam->decoder_context, sizeof(int64_t)*SHAPE_PRODUCT2(aam->decoder_context_shape),
                                                            aam->decoder_context_shape, 2,
                                                            ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64,
                                                            &aam->decoder_context_tensor));
    
    assert(aam->decoder_context_tensor != NULL);

    

    aam->hc_swap = false;
}

#include "tokens.h"
#define SEGSIZE 3200
void aam_feed_pcm16(AprilASRModel aam, short *pcm16, size_t short_count) {
    size_t head = 0;
    float wave[SEGSIZE];

    while(head < short_count){
        size_t remaining = short_count - head;
        if(remaining < 1) break;
        if(remaining > SEGSIZE) remaining = SEGSIZE;

        for(int i=0; i<remaining; i++){
            wave[i] = (float)pcm16[head + i] / 32768.0f;
        }
        
        fbank_accept_waveform(aam->fbank, wave, remaining);

        while(fbank_pull_segments( aam->fbank, aam->x_data, sizeof(float)*SHAPE_PRODUCT3(aam->x_shape) )){
            // Run encoder
            //printf("Run encoder\n");
            {
                aam->hc_swap = !aam->hc_swap;
                const char* input_names[] = {"x", "x_lens", "h", "c"};
                OrtValue *inputs[] = {
                    aam->x_tensor,
                    aam->x_len_tensor,
                    aam->hc_swap ? aam->h_1_tensor : aam->h_0_tensor,
                    aam->hc_swap ? aam->c_1_tensor : aam->c_0_tensor
                };

                const char* output_names[] = {"encoder_out", "encoder_out_lens", "next_h", "next_c"};
                OrtValue *outputs[] = {
                    aam->encoder_out3,
                    aam->encoder_out_lens,
                    aam->hc_swap ? aam->h_0_tensor : aam->h_1_tensor,
                    aam->hc_swap ? aam->c_0_tensor : aam->c_1_tensor
                };

                ORT_ABORT_ON_ERROR(g_ort->Run(aam->s_encoder, NULL,
                                                input_names, inputs, 4,
                                                output_names, 4, outputs));

                aam->encoder_out3 = outputs[0];
                aam->encoder_out_lens = outputs[1];

                
                OrtTensorTypeAndShapeInfo *info;
                ORT_ABORT_ON_ERROR(g_ort->GetTensorTypeAndShape(aam->encoder_out3, &info));
                
                size_t dimsize;
                ORT_ABORT_ON_ERROR(g_ort->GetTensorShapeElementCount(info, &dimsize));

                //printf("dimsize %d\n", dimsize);
                assert(dimsize == 1 * 512);
            }

            // encoder puts out (N, T, 512), projector needs (T, 512)
            //printf("Create encoder_out2\n");
            if(aam->encoder_out2 == NULL){
                float *data;
                ORT_ABORT_ON_ERROR(g_ort->GetTensorMutableData(aam->encoder_out3, &data));

                int64_t dims[2] = {1, 512};
                ORT_ABORT_ON_ERROR(g_ort->CreateTensorWithDataAsOrtValue(aam->memory_info, 
                                                            data, 1 * 512 * sizeof(float),
                                                            dims, 2,
                                                            ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
                                                            &aam->encoder_out2));
                
                assert(aam->encoder_out2 != NULL);
            }

            // Run encoder projector
            //printf("Run encoder projector\n");
            {
                const char* input_names[] = {"encoder_out"};
                OrtValue *inputs[] = { aam->encoder_out2 };

                const char* output_names[] = {"projected_encoder_out"};
                OrtValue *outputs[] = { aam->projected_encoder_out };

                ORT_ABORT_ON_ERROR(g_ort->Run(aam->s_joiner_encoder_proj, NULL,
                                                input_names, inputs, 1,
                                                output_names, 1, outputs));

                aam->projected_encoder_out = outputs[0];
            }

            //printf("decoder loop start\n");
            for(int i=0; i<8; i++){
                if(i > 6){
                    printf("\nUnreasonably high token output!\n");
                    break;
                }
                // Run decoder
                //printf("Run decoder\n");
                {
                    const char* input_names[] = {"y"};
                    OrtValue *inputs[] = { aam->decoder_context_tensor };

                    const char* output_names[] = {"decoder_out"};
                    OrtValue *outputs[] = { aam->decoder_out3 };

                    ORT_ABORT_ON_ERROR(g_ort->Run(aam->s_decoder, NULL,
                                                    input_names, inputs, 1,
                                                    output_names, 1, outputs));

                    aam->decoder_out3 = outputs[0];


                    OrtTensorTypeAndShapeInfo *info;
                    ORT_ABORT_ON_ERROR(g_ort->GetTensorTypeAndShape(aam->decoder_out3, &info));
                    
                    size_t dimsize;
                    ORT_ABORT_ON_ERROR(g_ort->GetTensorShapeElementCount(info, &dimsize));

                    //printf("decoder dimsize %d\n", dimsize);
                    assert(dimsize == 1 * 512);
                }

                // encoder puts out (N, T, 512), projector needs (T, 512)
                //printf("Create decoder_out2\n");
                if(aam->decoder_out2 == NULL){
                    float *data;
                    ORT_ABORT_ON_ERROR(g_ort->GetTensorMutableData(aam->decoder_out3, &data));

                    int64_t dims[2] = {1, 512};
                    ORT_ABORT_ON_ERROR(g_ort->CreateTensorWithDataAsOrtValue(aam->memory_info, 
                                                                data, 1 * 512 * sizeof(float),
                                                                dims, 2,
                                                                ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
                                                                &aam->decoder_out2));
                    
                    assert(aam->encoder_out2 != NULL);
                }

                // Run decoder projector
                //printf("Run decoder projector\n");
                {
                    const char* input_names[] = {"decoder_out"};
                    OrtValue *inputs[] = { aam->decoder_out2 };

                    const char* output_names[] = {"projected_decoder_out"};
                    OrtValue *outputs[] = { aam->projected_decoder_out };

                    ORT_ABORT_ON_ERROR(g_ort->Run(aam->s_joiner_decoder_proj, NULL,
                                                    input_names, inputs, 1,
                                                    output_names, 1, outputs));

                    aam->projected_decoder_out = outputs[0];
                }

                // Run joiner
                //printf("Run joiner\n");
                {
                    const char* input_names[] = {"projected_encoder_out", "projected_decoder_out"};
                    OrtValue *inputs[] = { aam->projected_encoder_out, aam->projected_decoder_out };

                    const char* output_names[] = {"logit"};
                    OrtValue *outputs[] = { aam->logits };

                    ORT_ABORT_ON_ERROR(g_ort->Run(aam->s_joiner, NULL,
                                                    input_names, inputs, 2,
                                                    output_names, 1, outputs));

                    aam->logits = outputs[0];
                }


                // Process logits
                //printf("Process logits\n");
                {
                    float *logits;
                    ORT_ABORT_ON_ERROR(g_ort->GetTensorMutableData(aam->logits, &logits));

                    //logits[0] -= 5.0f;

                    int max_idx = -1;
                    float max_val = -9999999.0;
                    for(int i=0; i<500; i++){
                        if(logits[i] > max_val){
                            max_idx = i;
                            max_val = logits[i];
                        }
                    }

                    //printf("Max idx %d with %.2f\n", max_idx, max_val);
                    if(max_idx != 0) {
                        //printf("%s ", tokens[max_idx]);
                        aam->active_tokens[aam->active_token_head] = max_idx;
                        aam->active_token_head++;

                        for(int m=0; m<aam->active_token_head; m++){
                            printf("%s", tokens[aam->active_tokens[m]]);
                        }
                        printf("\n");
                        if(aam->active_token_head > 16){
                            aam->active_token_head = 0;
                        }

                        aam->decoder_context[0] = aam->decoder_context[1];
                        aam->decoder_context[1] = (int64_t)max_idx;
                    }

                    if(max_idx == 0){
                        break;
                    }
                }

                //printf("Next\n");
            }
        }

        head += remaining;
    }
}