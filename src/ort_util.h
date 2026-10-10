/*
 * Copyright (C) 2022 abb128
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

#ifndef _APRIL_ORT_UTIL
#define _APRIL_ORT_UTIL

#include <stdio.h>
#include <assert.h>
#include "common.h"
#include "onnxruntime_c_api.h"
#include "file/model_file.h"
#include "log.h"

extern const OrtApi* g_ort;

#define ORT_ABORT_ON_ERROR(expr)                             \
  do {                                                       \
    OrtStatus* onnx_status = (expr);                         \
    if (onnx_status != NULL) {                               \
      const char* msg = g_ort->GetErrorMessage(onnx_status); \
      LOG_ERROR("ONNX: %s", msg);                            \
      g_ort->ReleaseStatus(onnx_status);                     \
      abort();                                               \
    }                                                        \
  } while (0);

#define PRINT_SHAPE1(MSG, shape) LOG_DEBUG(MSG "(%d,)", shape[0])
#define PRINT_SHAPE2(MSG, shape) LOG_DEBUG(MSG "(%d, %d)", shape[0], shape[1])
#define PRINT_SHAPE3(MSG, shape) LOG_DEBUG(MSG "(%d, %d, %d)", shape[0], shape[1], shape[2])

#define SHAPE_PRODUCT1(shape) (shape[0])
#define SHAPE_PRODUCT2(shape) (shape[0] * shape[1])
#define SHAPE_PRODUCT3(shape) (shape[0] * shape[1] * shape[2])

#define CALLOC_SHAPE1(SHAPE, TYPE) (TYPE *)calloc(SHAPE_PRODUCT1(SHAPE), sizeof(TYPE))
#define CALLOC_SHAPE2(SHAPE, TYPE) (TYPE *)calloc(SHAPE_PRODUCT2(SHAPE), sizeof(TYPE))
#define CALLOC_SHAPE3(SHAPE, TYPE) (TYPE *)calloc(SHAPE_PRODUCT3(SHAPE), sizeof(TYPE))

/* Recoverable setup failures must reach the API caller, not abort the host. */
static inline bool ort_ok(OrtStatus *status) {
    if (!status) return true;
    LOG_ERROR("ONNX: %s", g_ort->GetErrorMessage(status));
    g_ort->ReleaseStatus(status);
    return false;
}

typedef struct TensorF {
    float *data;
    OrtValue *tensor;
} TensorF;

typedef struct TensorI {
    int64_t *data;
    OrtValue *tensor;
} TensorI;

#define DEF_ALLOC_TENS(rtype, fname, rank, dtype, denum) \
    static inline rtype fname(OrtMemoryInfo *mi, int64_t *shape) { \
        rtype result = {0}; \
        size_t count = 1; \
        for (size_t i = 0; i < rank; ++i) { \
            if (shape[i] <= 0 || (uint64_t)shape[i] > SIZE_MAX / sizeof(dtype) / count) return result; \
            count *= (size_t)shape[i]; \
        } \
        result.data = calloc(count, sizeof(dtype)); \
        if (result.data && !ort_ok(g_ort->CreateTensorWithDataAsOrtValue( \
                mi, result.data, count * sizeof(dtype), shape, rank, denum, &result.tensor))) { \
            free(result.data); result.data = NULL; \
        } \
        return result; \
    }
DEF_ALLOC_TENS(TensorF, alloc_tensor3f, 3, float, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT)
DEF_ALLOC_TENS(TensorI, alloc_tensor2i, 2, int64_t, ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64)

static inline void free_tensorf(TensorF *f) {
    g_ort->ReleaseValue(f->tensor);
    free(f->data);

    f->tensor = NULL;
    f->data = NULL;
}

static inline void free_tensori(TensorI *f) {
    g_ort->ReleaseValue(f->tensor);
    free(f->data);
    
    f->tensor = NULL;
    f->data = NULL;
}

#define SET_CONCAT_PATH(out_path, base, fname)          \
    do {                                                \
        memset(out_path, 0, sizeof(out_path));          \
        strcpy(out_path, base);                         \
        strcat(out_path, "/" fname);                    \
    } while (0)


bool tensor_info(OrtSession *session, bool output, size_t index, const char *name,
                 ONNXTensorElementDataType type, int64_t *dimensions, size_t rank);

static inline size_t input_count(OrtSession *session) {
    size_t num = SIZE_MAX;
    if (!ort_ok(g_ort->SessionGetInputCount(session, &num))) return SIZE_MAX;
    return num;
}
static inline size_t output_count(OrtSession *session) {
    size_t num = SIZE_MAX;
    if (!ort_ok(g_ort->SessionGetOutputCount(session, &num))) return SIZE_MAX;
    return num;
}

static inline bool load_network_from_model_file(const OrtEnv *env, const OrtSessionOptions *options,
                                                 ModelFile file, size_t index, OrtSession **session) {
    size_t size = model_network_size(file, index);
    if (!size) return false;
    void *network = malloc(size);
    if (!network) return false;
    bool ok = model_network_read(file, index, network, size) == size;
    if (ok) ok = ort_ok(g_ort->CreateSessionFromArray(env, network, size, options, session));
    free(network);
    return ok;
}

#endif
