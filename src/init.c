#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <stdbool.h>

#include "april_api.h"
#include "fbank.h"
#include "onnxruntime_c_api.h"
#include "ort_util.h"

const OrtApi* g_ort = NULL;

void aam_api_init(void){
    g_ort = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    if (!g_ort) {
        fprintf(stderr, "Failed to init ONNX Runtime engine.\n");
        exit(-1);
    }
}