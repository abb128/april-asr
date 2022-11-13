#include "ort_util.h"

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
    const OrtTensorTypeAndShapeInfo *tinfo;
    ORT_ABORT_ON_ERROR(g_ort->SessionGetOutputTypeInfo(session, idx, &info));
    ORT_ABORT_ON_ERROR(g_ort->CastTypeInfoToTensorInfo(info, &tinfo));
    assert(tinfo != NULL);
    ORT_ABORT_ON_ERROR(g_ort->GetDimensionsCount(tinfo, &num));
    assert(num == dim_size);

    ORT_ABORT_ON_ERROR(g_ort->GetDimensions(tinfo, dimensions, dim_size));
    g_ort->ReleaseTypeInfo(info);

    return num;
}
