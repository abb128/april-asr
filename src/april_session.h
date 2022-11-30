#ifndef _APRIL_SESSION
#define _APRIL_SESSION

#include <stdbool.h>
#include "ort_util.h"
#include "april_model.h"
#include "april_api.h"
#include "fbank.h"

#include "audio_provider.h"
#include "proc_thread.h"

#define MAX_ACTIVE_TOKENS 256

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

    bool sync;
    bool realtime; // TODO
    AudioProvider provider;
    ProcThread thread;

    AprilRecognitionResultHandler handler;
    void *userdata;
};

#endif