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

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>

#include "ggml.h"
#include "ggml-cpu.h"
#include "ggml-alloc.h"
#include "ggml-backend.h"

#include "gguf.h"

#include "fbank.h"
#include "params.h"
#include "model_impl.h"


#define PRINT_SHAPE(t, text) printf(text " shape: %d %d %d %d\n", t->ne[0], t->ne[1], t->ne[2], t->ne[3])

struct encoder_layer_tensors {
    struct ggml_tensor *lstm_weight_ih_l0;
    struct ggml_tensor *lstm_weight_hh_l0;
    struct ggml_tensor *lstm_weight_hr_l0;
    struct ggml_tensor *lstm_bias_ih_l0;
    struct ggml_tensor *lstm_bias_hh_l0;
    struct ggml_tensor *feed_forward_0_weight;
    struct ggml_tensor *feed_forward_0_bias;
    struct ggml_tensor *feed_forward_4_weight;
    struct ggml_tensor *feed_forward_4_bias;

    float norm_final_eps;
};


#define MAX_NUM_LAYERS 32
struct april_model_ggml {
    const char* name;
    const char* description;

    int num_layers;
    int batch_size;
    int segment_size;
    int segment_step;
    int mel_features;
    int sample_rate;
    int frame_shift_ms;
    int frame_length_ms;
    int round_pow2;
    int mel_low;
    int mel_high;
    int snip_edges;
    int token_count;
    int blank_id;

    size_t vocab_length;
    const char *vocab;

    int joiner_dim;
    int h_state_dim;
    int c_state_dim;

    struct ggml_tensor *encoder_embed_0_w;
    struct ggml_tensor *encoder_embed_0_b;
    struct ggml_tensor *encoder_embed_3_w;
    struct ggml_tensor *encoder_embed_3_b;
    struct ggml_tensor *encoder_embed_6_w;
    struct ggml_tensor *encoder_embed_6_b;
    struct ggml_tensor *encoder_embed_out_w;
    struct ggml_tensor *encoder_embed_out_b;
    struct encoder_layer_tensors layers[MAX_NUM_LAYERS];

    float encoder_embed_out_norm_eps;


    struct ggml_tensor *decoder_embd_weight;
    struct ggml_tensor *decoder_conv_weight;
    struct ggml_tensor *joiner_encoder_proj_weight;
    struct ggml_tensor *joiner_encoder_proj_bias;
    struct ggml_tensor *joiner_decoder_proj_weight;
    struct ggml_tensor *joiner_decoder_proj_bias;
    struct ggml_tensor *joiner_output_weight;
    struct ggml_tensor *joiner_output_bias;

    struct ggml_tensor *one;

    // the backend to perform the computation (CPU, CUDA, METAL)
    ggml_backend_t backend = NULL;

    // the backend buffer to storage the tensors data
    ggml_backend_buffer_t buffer;

    // the context to define the tensor information (dimensions, size, memory address)
    struct ggml_context * ctx;
    // gguf_context for freeing

};



// Model implementation --------------------------

struct ggml_tensor *double_swish_inplace(
    struct ggml_context *ctx,
    struct ggml_tensor *x,
    const april_model_ggml& model
) {
    struct ggml_tensor *sig = ggml_sigmoid(ctx, ggml_sub(ctx, x, model.one));
    return ggml_mul_inplace(ctx, x, sig);
}


struct ggml_cgraph * build_decoder_graph(
    const april_model_ggml& model,
    int batch_size
) {
    static size_t buf_size = ggml_tensor_overhead()*GGML_DEFAULT_GRAPH_SIZE + ggml_graph_overhead();
    static std::vector<uint8_t> buf(buf_size);
    size_t token_ctx_size = 2;

    struct ggml_init_params params0 = {
        /*.mem_size   =*/ buf_size,
        /*.mem_buffer =*/ buf.data(),
        /*.no_alloc   =*/ true, // the tensors will be allocated later by ggml_allocr_alloc_graph()
    };

    // create a temporally context to build the graph
    struct ggml_context * ctx = ggml_init(params0);

    struct ggml_cgraph  * gf = ggml_new_graph(ctx);

    struct ggml_tensor *tokens = ggml_new_tensor_1d(ctx, GGML_TYPE_I32, token_ctx_size * batch_size);
    ggml_set_input(tokens);
    ggml_set_name(tokens, "tokens");

    struct ggml_tensor *inpL = ggml_get_rows(ctx, model.decoder_embd_weight, tokens);

    int hidden_size = model.joiner_dim;
    inpL = ggml_reshape_3d(ctx, inpL, hidden_size, token_ctx_size, batch_size);

    struct ggml_tensor *kernel = model.decoder_conv_weight;

    inpL = ggml_cont(ctx, ggml_permute(ctx, inpL, 2, 0, 3, 1));
    struct ggml_tensor *product = ggml_mul(ctx, inpL, kernel);

    struct ggml_tensor *sum = ggml_sum_rows(ctx, product);

    struct ggml_tensor *x = ggml_reshape_3d(ctx,
        sum,
        hidden_size, 1, batch_size
    );

    x = ggml_relu(ctx, x);
    x = ggml_mul_mat(ctx, model.joiner_decoder_proj_weight, x);
    x = ggml_add(ctx, x, model.joiner_decoder_proj_bias);
    
    ggml_set_output(x);
    ggml_set_name(x, "dec_out");

    ggml_build_forward_expand(gf, x);

    ggml_free(ctx);
    return gf;
}


struct ggml_cgraph * build_joiner_graph(
    const april_model_ggml& model,
    int batch_size
) {
    static size_t buf_size = ggml_tensor_overhead()*GGML_DEFAULT_GRAPH_SIZE + ggml_graph_overhead();
    static std::vector<uint8_t> buf(buf_size);

    struct ggml_init_params params0 = {
        /*.mem_size   =*/ buf_size,
        /*.mem_buffer =*/ buf.data(),
        /*.no_alloc   =*/ true, // the tensors will be allocated later by ggml_allocr_alloc_graph()
    };

    // create a temporally context to build the graph
    struct ggml_context * ctx = ggml_init(params0);

    struct ggml_cgraph  * gf = ggml_new_graph(ctx);

    struct ggml_tensor *enc_out = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, model.joiner_dim, 1, batch_size);
    ggml_set_input(enc_out);
    ggml_set_name(enc_out, "enc_out");

    struct ggml_tensor *dec_out = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, model.joiner_dim, 1, batch_size);
    ggml_set_input(dec_out);
    ggml_set_name(dec_out, "dec_out");

    struct ggml_tensor *x = ggml_add(ctx, enc_out, dec_out);
    x = ggml_tanh(ctx, x);
    x = ggml_mul_mat(ctx, model.joiner_output_weight, x);
    x = ggml_add(ctx, x, model.joiner_output_bias);

    ggml_set_output(x);
    ggml_set_name(x, "logits");

    ggml_build_forward_expand(gf, x);

    ggml_free(ctx);
    return gf;
}

struct ggml_cgraph * build_encoder_graph(
    const april_model_ggml& model,
    int batch_size
) {
    static size_t buf_size = ggml_tensor_overhead()*GGML_DEFAULT_GRAPH_SIZE + ggml_graph_overhead();
    static std::vector<uint8_t> buf(buf_size);

    struct ggml_init_params params0 = {
        /*.mem_size   =*/ buf_size,
        /*.mem_buffer =*/ buf.data(),
        /*.no_alloc   =*/ true, // the tensors will be allocated later by ggml_allocr_alloc_graph()
    };

    // create a temporally context to build the graph
    struct ggml_context * ctx = ggml_init(params0);

    struct ggml_cgraph  * gf = ggml_new_graph(ctx);

    struct ggml_tensor *inp = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, model.mel_features, model.segment_size, batch_size);
    ggml_set_input(inp);
    ggml_set_name(inp, "input");

    
    struct ggml_tensor *h_states[model.num_layers];
    struct ggml_tensor *c_states[model.num_layers];
    for(int i=0; i<model.num_layers; i++){
        h_states[i] = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, model.h_state_dim, batch_size);
        c_states[i] = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, model.c_state_dim, batch_size);

        ggml_set_input(h_states[i]);
        ggml_format_name(h_states[i], "h_states.%d", i);

        ggml_set_input(c_states[i]);
        ggml_format_name(c_states[i], "c_states.%d", i);
    }

    struct ggml_tensor *next_h_states[model.num_layers];
    struct ggml_tensor *next_c_states[model.num_layers];


    struct ggml_tensor *x = ggml_reshape_4d(ctx, inp,
        model.mel_features, model.segment_size, 1, batch_size);
    
    
    x = ggml_conv_2d(ctx, model.encoder_embed_0_w, x, 1, 1, 0, 0, 1, 1);
    x = ggml_add(ctx, x, ggml_permute(ctx, model.encoder_embed_0_b, 2, 3, 1, 0));
    x = double_swish_inplace(ctx, x, model);

    x = ggml_conv_2d(ctx, model.encoder_embed_3_w, x, 2, 2, 0, 0, 1, 1);
    x = ggml_add(ctx, x, ggml_permute(ctx, model.encoder_embed_3_b, 2, 3, 1, 0));
    x = double_swish_inplace(ctx, x, model);

    x = ggml_conv_2d(ctx, model.encoder_embed_6_w, x, 2, 2, 0, 0, 1, 1);
    x = ggml_add(ctx, x, ggml_permute(ctx, model.encoder_embed_6_b, 2, 3, 1, 0));
    x = double_swish_inplace(ctx, x, model);

    int64_t b = x->ne[3];
    int64_t c = x->ne[2];
    int64_t t = x->ne[1];
    int64_t f = x->ne[0];

    x = ggml_permute(ctx, x, 0, 2, 1, 3);
    x = ggml_cont_3d(ctx, x, c * f, t, b);

    x = ggml_mul_mat(ctx, model.encoder_embed_out_w, x);

    x = ggml_add(ctx, x, model.encoder_embed_out_b);

    x = ggml_rms_norm(ctx, x, model.encoder_embed_out_norm_eps);

    x = ggml_permute(ctx, x, 0, 1, 3, 2); // T, N, C

    x = ggml_cont(ctx, x);
    
    assert(t == 1);

    x = ggml_reshape_2d(ctx, x, x->ne[0], b); // [BatchSize, HiddenSize]
    
    // hidden states???
    for(int T=0; T<t; T++){ 
        // TODO: t != 1

        for(int l=0; l<model.num_layers; l++) {
            struct ggml_tensor *h_state = h_states[l];
            struct ggml_tensor *c_state = c_states[l];

            const struct encoder_layer_tensors *layer = &model.layers[l];

            struct ggml_tensor *inp_gates = ggml_mul_mat(ctx, layer->lstm_weight_ih_l0, x);
            inp_gates = ggml_add(ctx, inp_gates, layer->lstm_bias_ih_l0);

            struct ggml_tensor *hid_gates = ggml_mul_mat(ctx, layer->lstm_weight_hh_l0, h_state);
            hid_gates = ggml_add(ctx, hid_gates, layer->lstm_bias_hh_l0);
            
            struct ggml_tensor *gates = ggml_add(ctx, inp_gates, hid_gates);

            int64_t hidden_dim = layer->lstm_weight_hh_l0->ne[1] / 4;

            struct ggml_tensor *i_t = ggml_sigmoid(ctx, ggml_view_2d(ctx, gates, hidden_dim, b, gates->nb[1], 0 * sizeof(float) * hidden_dim));
            struct ggml_tensor *f_t = ggml_sigmoid(ctx, ggml_view_2d(ctx, gates, hidden_dim, b, gates->nb[1], 1 * sizeof(float) * hidden_dim));
            struct ggml_tensor *g_t = ggml_tanh(ctx   , ggml_view_2d(ctx, gates, hidden_dim, b, gates->nb[1], 2 * sizeof(float) * hidden_dim));
            struct ggml_tensor *o_t = ggml_sigmoid(ctx, ggml_view_2d(ctx, gates, hidden_dim, b, gates->nb[1], 3 * sizeof(float) * hidden_dim));


            struct ggml_tensor *fc = ggml_cont(ctx,  ggml_mul(ctx, f_t, c_state));
            struct ggml_tensor *ig = ggml_cont(ctx,  ggml_mul(ctx, i_t, g_t));

            struct ggml_tensor *next_c = ggml_add(ctx, fc, ig);

            struct ggml_tensor *h_tilde = ggml_mul(ctx, o_t, ggml_tanh(ctx, next_c));

            struct ggml_tensor *next_h = ggml_mul_mat(ctx, layer->lstm_weight_hr_l0, h_tilde);

            struct ggml_tensor *x_lstm = next_h;

            x = ggml_add(ctx, x, x_lstm);

            struct ggml_tensor *x_ff = ggml_mul_mat(ctx, layer->feed_forward_0_weight, x);
            x_ff = ggml_add(ctx, x_ff, layer->feed_forward_0_bias);
            x_ff = double_swish_inplace(ctx, x_ff, model);
            x_ff = ggml_mul_mat(ctx, layer->feed_forward_4_weight, x_ff);
            x_ff = ggml_add(ctx, x_ff, layer->feed_forward_4_bias);

            x = ggml_add(ctx, x, x_ff);

            x = ggml_rms_norm(ctx, x, layer->norm_final_eps);

            next_h_states[l] = next_h;
            next_c_states[l] = next_c;
        }
    }
    
    x = ggml_reshape_3d(ctx, x, x->ne[0], 1, b);

    x = ggml_mul_mat(ctx, model.joiner_encoder_proj_weight, x);
    x = ggml_add(ctx, x, model.joiner_encoder_proj_bias);

    x = ggml_cont(ctx, x);
    
    ggml_set_output(x);
    ggml_set_name(x, "enc_out");

    for(int i=0; i<model.num_layers; i++){
        ggml_set_output(next_h_states[i]);
        ggml_format_name(next_h_states[i], "next_h_states.%d", i);

        ggml_set_output(next_c_states[i]);
        ggml_format_name(next_c_states[i], "next_c_states.%d", i);
    }

    ggml_build_forward_expand(gf, x);

    ggml_free(ctx);
    return gf;
}

bool compute_encoder(
    const april_model_ggml & model,
    ggml_gallocr_t allocr,
    int batch_size,
    float **inputs,
    float **h_states,
    float **c_states,
    float **enc_outs
) {
    struct ggml_cgraph * gf = build_encoder_graph(model, batch_size);
    
    ggml_gallocr_alloc_graph(allocr, gf);

    struct ggml_tensor * graph_input = ggml_graph_get_tensor(gf, "input");

    for(int i=0; i<batch_size; i++) {
        ggml_backend_tensor_set(
            /*    dst */ graph_input,
            /*    src */ inputs[i],
            /* offset */ i * model.segment_size * model.mel_features * sizeof(float),
            /*   size */ model.segment_size * model.mel_features * sizeof(float)
        );
    }

    for(int i=0; i<model.num_layers; i++) {
        std::string h_name = "h_states." + std::to_string(i);
        std::string c_name = "c_states." + std::to_string(i);

        struct ggml_tensor * graph_h = ggml_graph_get_tensor(gf, h_name.c_str());
        struct ggml_tensor * graph_c = ggml_graph_get_tensor(gf, c_name.c_str());
        for(int j=0; j<batch_size; j++) {
            ggml_backend_tensor_set(
                /*    dst */ graph_h,
                /*    src */ &h_states[j][i * model.h_state_dim],
                /* offset */ j * model.h_state_dim * sizeof(float),
                /*   size */ model.h_state_dim * sizeof(float)
            );

            ggml_backend_tensor_set(
                /*    dst */ graph_c,
                /*    src */ &c_states[j][i * model.c_state_dim],
                /* offset */ j * model.c_state_dim * sizeof(float),
                /*   size */ model.c_state_dim * sizeof(float)
            );
        }
    }

    ggml_backend_cpu_set_n_threads(model.backend, 8);
    ggml_backend_graph_compute(model.backend, gf);


    for(int i=0; i<model.num_layers; i++) {
        std::string h_name = "next_h_states." + std::to_string(i);
        std::string c_name = "next_c_states." + std::to_string(i);

        struct ggml_tensor * graph_h = ggml_graph_get_tensor(gf, h_name.c_str());
        struct ggml_tensor * graph_c = ggml_graph_get_tensor(gf, c_name.c_str());
        for(int j=0; j<batch_size; j++) {
            ggml_backend_tensor_get(
                /*    src */ graph_h,
                /*   dest */ &h_states[j][i * model.h_state_dim],
                /* offset */ j * model.h_state_dim * sizeof(float),
                /*   size */ model.h_state_dim * sizeof(float)
            );

            ggml_backend_tensor_get(
                /*    src */ graph_c,
                /*   dest */ &c_states[j][i * model.c_state_dim],
                /* offset */ j * model.c_state_dim * sizeof(float),
                /*   size */ model.c_state_dim * sizeof(float)
            );
        }
    }

    struct ggml_tensor * enc_out = ggml_graph_get_tensor(gf, "enc_out");
    for(int j=0; j<batch_size; j++) {
        ggml_backend_tensor_get(
            /*    src */ enc_out,
            /*   dest */ enc_outs[j],
            /* offset */ j * model.joiner_dim * sizeof(float),
            /*   size */ model.joiner_dim * sizeof(float)
        );
    }

    return true;
}

bool compute_decoder(
    const april_model_ggml & model,
    ggml_gallocr_t allocr,
    //std::vector<std::pair<int, int>> tokens
    int batch_size,
    int **token_ctx,
    float **dec_outs
) {
    struct ggml_cgraph * gf = build_decoder_graph(model, batch_size);

    ggml_gallocr_alloc_graph(allocr, gf);

    struct ggml_tensor * graph_tokens = ggml_graph_get_tensor(gf, "tokens");
    for(int b=0; b<batch_size; b++) {
        ggml_backend_tensor_set(
            graph_tokens,
            &(token_ctx[b][0]),
            (2 * b) * sizeof(int32_t),
            sizeof(int32_t)
        );
        ggml_backend_tensor_set(
            graph_tokens,
            &(token_ctx[b][1]),
            (2 * b + 1) * sizeof(int32_t),
            sizeof(int32_t)
        );
    }
    
    ggml_backend_cpu_set_n_threads(model.backend, 8);
    ggml_backend_graph_compute(model.backend, gf);

    struct ggml_tensor * dec_out = ggml_graph_get_tensor(gf, "dec_out");
    for(int j=0; j<batch_size; j++) {
        ggml_backend_tensor_get(
            /*    src */ dec_out,
            /*   dest */ dec_outs[j],
            /* offset */ j * model.joiner_dim * sizeof(float),
            /*   size */ model.joiner_dim * sizeof(float)
        );
    }

    /*
    #ifdef APRIL_DEBUG_SAVE_AUDIO
    {
        if(fd == NULL) fd = fopen("/tmp/aas_debug.dbgOut.bin", "w");
        std::vector<int32_t> tmp;
        struct ggml_tensor * dbgOut = ggml_graph_get_tensor(gf, "dbgOut");
        PRINT_SHAPE(dbgOut, "dbgOut");
        printf("dbgOut type %s\n", ggml_type_name(dbgOut->type));
        tmp.resize(ggml_nelements(dbgOut));
        ggml_backend_tensor_get(dbgOut, tmp.data(), 0, ggml_nbytes(dbgOut));
        printf("tmp values %d %d %d\n", ((int32_t *)dbgOut->data)[0], tmp[1], tmp[2]);
        fwrite(tmp.data(), 1, ggml_nbytes(dbgOut), fd);
        fflush(fd);
        exit(1);
    }
    #endif
    */


    return true;
}


bool compute_joiner(
    const april_model_ggml & model,
    ggml_gallocr_t allocr,
    int batch_size,
    float **enc_outs,
    float **dec_outs,
    float **logits
) {
    struct ggml_cgraph * gf = build_joiner_graph(model, batch_size);
    
    ggml_gallocr_alloc_graph(allocr, gf);

    struct ggml_tensor * graph_enc_out = ggml_graph_get_tensor(gf, "enc_out");
    struct ggml_tensor * graph_dec_out = ggml_graph_get_tensor(gf, "dec_out");

    for(int i=0; i<batch_size; i++) {
        ggml_backend_tensor_set(
            /*    dst */ graph_enc_out,
            /*    src */ enc_outs[i],
            /* offset */ i * model.joiner_dim * sizeof(float),
            /*   size */ model.joiner_dim * sizeof(float)
        );
    }

    for(int i=0; i<batch_size; i++) {
        ggml_backend_tensor_set(
            /*    dst */ graph_dec_out,
            /*    src */ dec_outs[i],
            /* offset */ i * model.joiner_dim * sizeof(float),
            /*   size */ model.joiner_dim * sizeof(float)
        );
    }

    ggml_backend_cpu_set_n_threads(model.backend, 8);
    ggml_backend_graph_compute(model.backend, gf);

    struct ggml_tensor * graph_logits = ggml_graph_get_tensor(gf, "logits");
    for(int j=0; j<batch_size; j++) {
        ggml_backend_tensor_get(
            /*    src */ graph_logits,
            /*   dest */ logits[j],
            /* offset */ j * model.token_count * sizeof(float),
            /*   size */ model.token_count * sizeof(float)
        );
    }

    /*
    for(int b=0; b<batch_size; b++) {
        printf("[%d] join enc inp %2.2f %2.2f\n", b, enc_outs[b][0], enc_outs[b][1]);
    }

    for(int b=0; b<batch_size; b++) {
        printf("[%d] join dec inp %2.2f %2.2f\n", b, dec_outs[b][0], dec_outs[b][1]);
    }

    for(int b=0; b<batch_size; b++) {
        printf("[%d] logits out %2.2f %2.2f\n", b, logits[b][0], logits[b][1]);
    }*/


    return true;
}








// End Model implementation --------------------------









struct april_context_ggml {
    struct april_model_ggml *model;
    ggml_gallocr_t allocr_enc;
    ggml_gallocr_t allocr_dec;
    ggml_gallocr_t allocr_joi;
};

struct april_model_ggml *load_model(const char *path) {
    struct april_model_ggml *model = (struct april_model_ggml *)malloc(sizeof(struct april_model_ggml));
    
    // TODO: Other backends

    //if (!model->backend) {
    model->backend = ggml_backend_cpu_init();
    //}

    struct ggml_init_params params {
            /*.mem_size   =*/ 1024,
            /*.mem_buffer =*/ NULL,
            /*.no_alloc   =*/ false,
    };

    // create context
    struct ggml_context *ctx = ggml_init(params);
    model->ctx = ctx;
    model->one = ggml_new_f32(ctx, 1.0f);

    struct gguf_init_params ggufparams = {
        /*.no_alloc = */ false,
        /*.ctx      = */ &ctx,
    };
    struct gguf_context * gctx = gguf_init_from_file(path, ggufparams);


    const char *arch = gguf_get_val_str(gctx, gguf_find_key(gctx, "general.architecture"));
    if(strcmp(arch, "april") != 0) {
        free(model);
        gguf_free(gctx);
        ggml_free(ctx);
        // Log error
        return NULL;
    }

    model->name = gguf_get_val_str(gctx, gguf_find_key(gctx, "general.name"));
    model->description = gguf_get_val_str(gctx, gguf_find_key(gctx, "general.description"));

    model->num_layers      = gguf_get_val_u32(gctx, gguf_find_key(gctx, "layer_count"));
    model->batch_size      = gguf_get_val_u32(gctx, gguf_find_key(gctx, "batch_size"));
    model->segment_size    = gguf_get_val_u32(gctx, gguf_find_key(gctx, "segment_size"));
    model->segment_step    = gguf_get_val_u32(gctx, gguf_find_key(gctx, "segment_step"));
    model->mel_features    = gguf_get_val_u32(gctx, gguf_find_key(gctx, "mel_features"));
    model->sample_rate     = gguf_get_val_u32(gctx, gguf_find_key(gctx, "sample_rate"));
    model->frame_shift_ms  = gguf_get_val_u32(gctx, gguf_find_key(gctx, "frame_shift_ms"));
    model->frame_length_ms = gguf_get_val_u32(gctx, gguf_find_key(gctx, "frame_length_ms"));
    model->round_pow2      = gguf_get_val_u32(gctx, gguf_find_key(gctx, "round_pow2"));
    model->mel_low         = gguf_get_val_u32(gctx, gguf_find_key(gctx, "mel_low"));
    model->mel_high        = gguf_get_val_u32(gctx, gguf_find_key(gctx, "mel_high"));
    model->snip_edges      = gguf_get_val_u32(gctx, gguf_find_key(gctx, "snip_edges"));
    model->token_count     = gguf_get_val_u32(gctx, gguf_find_key(gctx, "token_count"));
    model->blank_id        = gguf_get_val_u32(gctx, gguf_find_key(gctx, "blank_id"));
    model->encoder_embed_out_norm_eps = std::exp(gguf_get_val_f32(gctx, gguf_find_key(gctx, "encoder_embed_out_norm_eps")));

    std::map<std::string, struct ggml_tensor *> weights_map;
    for (ggml_tensor * cur = ggml_get_first_tensor(ctx); cur; cur = ggml_get_next_tensor(ctx, cur)) {
        std::string tensor_name = std::string(cur->name);
        weights_map.emplace(tensor_name, cur);
    }

    model->encoder_embed_0_w   = weights_map["encoder_embed_0_w"];
    model->encoder_embed_0_b   = weights_map["encoder_embed_0_b"];
    model->encoder_embed_3_w   = weights_map["encoder_embed_3_w"];
    model->encoder_embed_3_b   = weights_map["encoder_embed_3_b"];
    model->encoder_embed_6_w   = weights_map["encoder_embed_6_w"];
    model->encoder_embed_6_b   = weights_map["encoder_embed_6_b"];
    model->encoder_embed_out_w = weights_map["encoder_embed_out_w"];
    model->encoder_embed_out_b = weights_map["encoder_embed_out_b"];
    model->decoder_embd_weight        = weights_map["decoder_embd_weight"];
    model->decoder_conv_weight        = weights_map["decoder_conv_weight"];
    model->joiner_encoder_proj_weight = weights_map["joiner_encoder_proj_weight"];
    model->joiner_encoder_proj_bias   = weights_map["joiner_encoder_proj_bias"];
    model->joiner_decoder_proj_weight = weights_map["joiner_decoder_proj_weight"];
    model->joiner_decoder_proj_bias   = weights_map["joiner_decoder_proj_bias"];
    model->joiner_output_weight       = weights_map["joiner_output_weight"];
    model->joiner_output_bias         = weights_map["joiner_output_bias"];

    for(int i=0; i<model->num_layers; i++) {
        std::string is = std::to_string(i);
        model->layers[i].lstm_weight_ih_l0     = weights_map["encoder." + is + ".lstm.weight_ih_l0"];
        model->layers[i].lstm_weight_hh_l0     = weights_map["encoder." + is + ".lstm.weight_hh_l0"];
        model->layers[i].lstm_weight_hr_l0     = weights_map["encoder." + is + ".lstm.weight_hr_l0"];
        model->layers[i].lstm_bias_ih_l0       = weights_map["encoder." + is + ".lstm.bias_ih_l0"];
        model->layers[i].lstm_bias_hh_l0       = weights_map["encoder." + is + ".lstm.bias_hh_l0"];
        model->layers[i].feed_forward_0_weight = weights_map["encoder." + is + ".feed_forward.0.weight"];
        model->layers[i].feed_forward_0_bias   = weights_map["encoder." + is + ".feed_forward.0.bias"];
        model->layers[i].feed_forward_4_weight = weights_map["encoder." + is + ".feed_forward.4.weight"];
        model->layers[i].feed_forward_4_bias   = weights_map["encoder." + is + ".feed_forward.4.bias"];
        model->layers[i].norm_final_eps = std::exp(gguf_get_val_f32(gctx, gguf_find_key(gctx, ("encoder." + is + ".norm_final.eps").c_str())));
    }

    model->joiner_dim = model->joiner_output_weight->ne[0];
    model->h_state_dim = model->layers[0].lstm_weight_hr_l0->ne[1];
    model->c_state_dim = model->layers[0].lstm_weight_hr_l0->ne[0];

    assert(model->joiner_dim > 2 && model->joiner_dim < 16384);
    assert(model->h_state_dim > 2 && model->h_state_dim < 16384);
    assert(model->c_state_dim > 2 && model->c_state_dim < 16384);
    assert(model->token_count > 2 && model->token_count < 131072);
    assert(model->blank_id >= 0 && model->blank_id < model->token_count);
    assert(model->num_layers > 0 && model->num_layers <= MAX_NUM_LAYERS);
    assert(model->segment_size > 0 && model->segment_size <= 128);
    assert(model->segment_step > 0 && model->segment_step <= 128);
    assert(model->mel_features > 0 && model->mel_features <= 512);

    const int token_idx = gguf_find_key(gctx, "tokenizer.ggml.tokens");
    uint32_t n_tokens = gguf_get_arr_n(gctx, token_idx);

    assert(n_tokens == model->token_count);

    size_t max_token_len = 0;
    for(int i=0; i<n_tokens; i++) {
        max_token_len = std::max(max_token_len, strlen(gguf_get_arr_str(gctx, token_idx, i)));
    }

    model->vocab_length = max_token_len;
    char *tokens = (char *)calloc(max_token_len, model->token_count);
    for(int i=0; i<model->token_count; i++) {
        strncpy(&tokens[i * max_token_len], gguf_get_arr_str(gctx, token_idx, i), max_token_len);
    }
    model->vocab = tokens;

    return model;
}

void model_alloc_tensors(struct april_model_ggml *model, struct april_model_tensors *tensors) {
    tensors->h_state = (float *)calloc(sizeof(float), model->h_state_dim * model->num_layers);
    tensors->c_state = (float *)calloc(sizeof(float), model->c_state_dim * model->num_layers);
    tensors->enc_out = (float *)calloc(sizeof(float), model->joiner_dim);
    tensors->dec_out = (float *)calloc(sizeof(float), model->joiner_dim);
    tensors->logits  = (float *)calloc(sizeof(float), model->token_count);
    tensors->enc_inp_size = model->mel_features * model->segment_size * sizeof(float);
    tensors->enc_inp = (float *)calloc(1, tensors->enc_inp_size);
}

const char * model_get_name(struct april_model_ggml *model) {
    return model->name;
}

const char * model_get_desc(struct april_model_ggml *model) {
    return model->description;
}

void model_get_params(struct april_model_ggml *model, ModelParameters *out) {
    out->batch_size      = -1; // any is acceptable
    out->segment_size    = model->segment_size;
    out->segment_step    = model->segment_step;
    out->mel_features    = model->mel_features;
    out->sample_rate     = model->sample_rate;
    out->frame_shift_ms  = model->frame_shift_ms;
    out->frame_length_ms = model->frame_length_ms;
    out->round_pow2      = model->round_pow2;
    out->mel_low         = model->mel_low;
    out->mel_high        = model->mel_high;
    out->snip_edges      = model->snip_edges;
    out->token_count     = model->token_count;
    out->blank_id        = model->blank_id;
    out->token_length    = model->vocab_length;
    out->tokens          = model->vocab;
}


void model_get_fbank_opts(struct april_model_ggml *model, FBankOptions *out) {
    out->sample_freq = model->sample_rate;
    out->frame_shift_ms = model->frame_shift_ms;
    out->frame_length_ms = model->frame_length_ms;
    out->num_bins = model->mel_features;
    out->round_pow2 = model->round_pow2;
    out->mel_low = model->mel_low;
    out->mel_high = model->mel_high;
    out->snip_edges = model->snip_edges == 1;
    out->pull_segment_count = model->segment_size;
    out->pull_segment_step = model->segment_step;
}

struct april_context_ggml *init_context(struct april_model_ggml *model, int max_batch_size) {
    struct april_context_ggml *ctx = (struct april_context_ggml *)malloc(sizeof(struct april_context_ggml));

    ggml_gallocr_t allocr_enc = NULL;
    ggml_gallocr_t allocr_dec = NULL;
    ggml_gallocr_t allocr_joi = NULL;
    {
        allocr_enc = ggml_gallocr_new(ggml_backend_get_default_buffer_type(model->backend));
        allocr_dec = ggml_gallocr_new(ggml_backend_get_default_buffer_type(model->backend));
        allocr_joi = ggml_gallocr_new(ggml_backend_get_default_buffer_type(model->backend));

        struct ggml_cgraph *gf1 = build_encoder_graph(*model, max_batch_size);
        struct ggml_cgraph *gf2 = build_decoder_graph(*model, max_batch_size);
        struct ggml_cgraph *gf3 =  build_joiner_graph(*model, max_batch_size);
        ggml_gallocr_reserve(allocr_enc, gf1);
        ggml_gallocr_reserve(allocr_dec, gf2);
        ggml_gallocr_reserve(allocr_joi, gf3);
        size_t mem_size1 = ggml_gallocr_get_buffer_size(allocr_enc, 0);
        size_t mem_size2 = ggml_gallocr_get_buffer_size(allocr_dec, 0);
        size_t mem_size3 = ggml_gallocr_get_buffer_size(allocr_joi, 0);
        fprintf(stderr, "%s: compute buffer size: %.2f MB\n", __func__, mem_size1/1024.0/1024.0);
        fprintf(stderr, "%s: compute buffer size: %.2f MB\n", __func__, mem_size2/1024.0/1024.0);
        fprintf(stderr, "%s: compute buffer size: %.2f MB\n", __func__, mem_size3/1024.0/1024.0);
    }
   
    ctx->allocr_enc = allocr_enc;
    ctx->allocr_dec = allocr_dec;
    ctx->allocr_joi = allocr_joi;
    ctx->model = model;

    return ctx;
}

bool run_encoder(struct april_context_ggml *ctx, int batch_size, float **inputs, float **h_states, float **c_states, float **outputs) {
    return compute_encoder(
        *ctx->model,
        ctx->allocr_enc,
        batch_size,
        inputs,
        h_states,
        c_states,
        outputs
    );
}

bool run_decoder(struct april_context_ggml *ctx, int batch_size, int **token_ctx, float **outputs) {
    return compute_decoder(
        *ctx->model,
        ctx->allocr_dec,
        batch_size,
        token_ctx,
        outputs
    );
}

bool run_joiner(struct april_context_ggml *ctx, int batch_size, float **enc_out, float **dec_out, float **logits) {
    return compute_joiner(
        *ctx->model,
        ctx->allocr_joi,
        batch_size,
        enc_out,
        dec_out,
        logits
    );
}