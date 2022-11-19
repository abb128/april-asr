#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <stdbool.h>

#include "april_api.h"
#include "fbank.h"
#include "onnxruntime_c_api.h"
#include "ort_util.h"
#include "log.h"

const OrtApi* g_ort = NULL;
LogLevel g_loglevel = LEVEL_WARNING;

void aam_api_init(void){
    char *log_env = getenv("APRIL_LOG_LEVEL");
    if(log_env){
        for(int i=0; i<=LEVEL_COUNT; i++){
            if(strcmp(log_env, LogLevelStrings[i]) == 0) {
                g_loglevel = (LogLevel)i;
            }
        }
        LOG_DEBUG("Using LogLevel %d", g_loglevel);
    }

    g_ort = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    if (!g_ort) {
        LOG_CRITICAL("Failed to init ONNX Runtime engine!");
        exit(-1);
    }

    LOG_DEBUG("This is how a debug log looks like. %d", 123456);
    LOG_INFO("This is how an info log looks like. %d", 123456);
    LOG_WARNING("This is how a warning log looks like. %d", 123456);
    LOG_ERROR("This is how an error log looks like. %d", 123456);
    LOG_CRITICAL("This is how a critical log looks like. %d", 123456);
}