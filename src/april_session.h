#ifndef _APRIL_SESSION
#define _APRIL_SESSION

#include <stdbool.h>
#include "ort_util.h"
#include "april_model.h"
#include "april_api.h"
#include "fbank.h"

#define MAX_ACTIVE_TOKENS 144

struct AprilASRSession_i {
    AprilASRModel model;
    OnlineFBank fbank;

    OrtMemoryInfo *memory_info;

    TensorF x;

    bool hc_use_0;
    TensorF h[2];
    TensorF c[2];

    TensorF eout;

    size_t context_size;
    TensorI context;
    TensorF dout;
    bool dout_init;

    TensorF logits;

    AprilToken active_tokens[MAX_ACTIVE_TOKENS];
    size_t active_token_head;
    size_t last_handler_call_head;

    bool was_flushed;
    uint64_t runs_since_emission;

    AprilRecognitionResultHandler handler;
    void *userdata;
};

#endif