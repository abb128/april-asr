#ifndef _APRIL_MODEL
#define _APRIL_MODEL

#include "ort_util.h"
#include "april_api.h"
#include "params.h"
#include "fbank.h"
struct AprilASRModel_i {
    OrtEnv *env;
    OrtSessionOptions* session_options;

    OrtSession* encoder;
    OrtSession* decoder;
    OrtSession* joiner;

    // The comment numbers are for reference only, it may differ
    // with different sized models.
    int64_t x_dim[3];       // (1, 9, 80)
    int64_t h_dim[3];       // (12, 1, 512)
    int64_t c_dim[3];       // (12, 1, 1024)
    int64_t eout_dim[3];    // (1, 1, 512)
    int64_t dout_dim[3];    // (1, 1, 512)
    int64_t context_dim[2]; // (1, 2)
    int64_t logits_dim[3];  // (1, 1, 500)

    FBankOptions fbank_opts;
    ModelParameters params;
};

#endif