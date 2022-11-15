#ifndef _APRIL_SESSION
#define _APRIL_SESSION

#include <stdbool.h>
#include "ort_util.h"
#include "april_model.h"
#include "april_api.h"
#include "fbank.h"

struct AprilASRSession_i {
    AprilASRModel model;
    OnlineFBank fbank;

    OrtMemoryInfo *memory_info;

    TensorF x;

    bool hc_use_0;
    TensorF h[2];
    TensorF c[2];

    TensorF eout;

    TensorI context;
    TensorF logits;

    int64_t active_tokens[64];
    size_t active_token_head;

    uint64_t runs_since_emission;

    AprilRecognitionResultHandler handler;
    void *userdata;
};

#endif