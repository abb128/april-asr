#include "onnxruntime_c_api.h"

extern const OrtApi* g_ort;

#define ORT_ABORT_ON_ERROR(expr)                             \
  do {                                                       \
    OrtStatus* onnx_status = (expr);                         \
    if (onnx_status != NULL) {                               \
      const char* msg = g_ort->GetErrorMessage(onnx_status); \
      fprintf(stderr, "ONNX: %s\n", msg);                    \
      g_ort->ReleaseStatus(onnx_status);                     \
      abort();                                               \
    }                                                        \
  } while (0);

#define PRINT_SHAPE1(MSG, shape) printf(MSG "(%d,)\n", shape[0])
#define PRINT_SHAPE2(MSG, shape) printf(MSG "(%d, %d)\n", shape[0], shape[1])
#define PRINT_SHAPE3(MSG, shape) printf(MSG "(%d, %d, %d)\n", shape[0], shape[1], shape[2])

#define SHAPE_PRODUCT1(shape) (shape[0])
#define SHAPE_PRODUCT2(shape) (shape[0] * shape[1])
#define SHAPE_PRODUCT3(shape) (shape[0] * shape[1] * shape[2])

#define CALLOC_SHAPE1(SHAPE, TYPE) (TYPE *)calloc(SHAPE_PRODUCT1(SHAPE), sizeof(TYPE))
#define CALLOC_SHAPE2(SHAPE, TYPE) (TYPE *)calloc(SHAPE_PRODUCT2(SHAPE), sizeof(TYPE))
#define CALLOC_SHAPE3(SHAPE, TYPE) (TYPE *)calloc(SHAPE_PRODUCT3(SHAPE), sizeof(TYPE))

#define CREATE_TENSOR1(MEMINFO, DATA, SHAPE, TYPE, TYPE_ENUM, OUT) \
    ORT_ABORT_ON_ERROR(g_ort->CreateTensorWithDataAsOrtValue((MEMINFO), (DATA), sizeof(TYPE)*SHAPE_PRODUCT1((SHAPE)), (SHAPE), 1, (TYPE_ENUM), (OUT)));

#define CREATE_TENSOR2(MEMINFO, DATA, SHAPE, TYPE, TYPE_ENUM, OUT) \
    ORT_ABORT_ON_ERROR(g_ort->CreateTensorWithDataAsOrtValue((MEMINFO), (DATA), sizeof(TYPE)*SHAPE_PRODUCT2((SHAPE)), (SHAPE), 2, (TYPE_ENUM), (OUT)));

#define CREATE_TENSOR3(MEMINFO, DATA, SHAPE, TYPE, TYPE_ENUM, OUT) \
    ORT_ABORT_ON_ERROR(g_ort->CreateTensorWithDataAsOrtValue((MEMINFO), (DATA), sizeof(TYPE)*SHAPE_PRODUCT3((SHAPE)), (SHAPE), 3, (TYPE_ENUM), (OUT)));

typedef struct TensorF {
    float *data;
    OrtValue *tensor;
} TensorF;

typedef struct TensorI {
    int64_t *data;
    OrtValue *tensor;
} TensorI;

#define DEF_ALLOC_TENS(rtype, fname, allocor, creator, dtype, denum)            \
    static inline rtype fname(OrtMemoryInfo *memory_info, int64_t *shape){      \
        rtype result;                                                           \
        result.data = allocor(shape, dtype);                                    \  
        creator(memory_info, result.data, shape, dtype, denum, &result.tensor); \
        return result;                                                          \
    }

DEF_ALLOC_TENS(TensorF, alloc_tensor1f, CALLOC_SHAPE1, CREATE_TENSOR1, float, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT);
DEF_ALLOC_TENS(TensorF, alloc_tensor2f, CALLOC_SHAPE2, CREATE_TENSOR2, float, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT);
DEF_ALLOC_TENS(TensorF, alloc_tensor3f, CALLOC_SHAPE3, CREATE_TENSOR3, float, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT);

DEF_ALLOC_TENS(TensorI, alloc_tensor1i, CALLOC_SHAPE1, CREATE_TENSOR1, int64_t, ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64);
DEF_ALLOC_TENS(TensorI, alloc_tensor2i, CALLOC_SHAPE2, CREATE_TENSOR2, int64_t, ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64);
DEF_ALLOC_TENS(TensorI, alloc_tensor3i, CALLOC_SHAPE3, CREATE_TENSOR3, int64_t, ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64);

void free_tensorf(TensorF *f) { } // TODO
void free_tensori(TensorI *f) { } // TODO

#define SET_CONCAT_PATH(out_path, base, fname)          \
    do {                                                \
        memset(out_path, 0, sizeof(out_path));          \
        strcpy(out_path, base);                         \
        strcat(out_path, "/" fname);                    \
    } while (0)

size_t input_dims(OrtSession* session, size_t idx, int64_t *dimensions, size_t dim_size) {
    size_t num;
    OrtTypeInfo *info;
    const OrtTensorTypeAndShapeInfo *tinfo;
    ORT_ABORT_ON_ERROR(g_ort->SessionGetInputTypeInfo(session, idx, &info));
    ORT_ABORT_ON_ERROR(g_ort->CastTypeInfoToTensorInfo(info, &tinfo));
    assert(tinfo != NULL);
    ORT_ABORT_ON_ERROR(g_ort->GetDimensionsCount(tinfo, &num));
    assert(num == dim_size);

    ORT_ABORT_ON_ERROR(g_ort->GetDimensions(tinfo, dimensions, dim_size));
    g_ort->ReleaseTypeInfo(info);

    return num;
}

size_t output_dims(OrtSession* session, size_t idx, int64_t *dimensions, size_t dim_size) {
    size_t num;
    OrtTypeInfo *info;
    OrtTensorTypeAndShapeInfo *tinfo;
    ORT_ABORT_ON_ERROR(g_ort->SessionGetOutputTypeInfo(session, idx, &info));
    ORT_ABORT_ON_ERROR(g_ort->CastTypeInfoToTensorInfo(info, &tinfo));
    assert(tinfo != NULL);
    ORT_ABORT_ON_ERROR(g_ort->GetDimensionsCount(tinfo, &num));
    assert(num == dim_size);

    ORT_ABORT_ON_ERROR(g_ort->GetDimensions(tinfo, dimensions, dim_size));
    g_ort->ReleaseTypeInfo(info);

    return num;
}

size_t input_count(OrtSession *session) {
    size_t num;
    ORT_ABORT_ON_ERROR(g_ort->SessionGetInputCount(session, &num));
    return num;
}

size_t output_count(OrtSession *session) {
    size_t num;
    ORT_ABORT_ON_ERROR(g_ort->SessionGetOutputCount(session, &num));
    return num;
}
