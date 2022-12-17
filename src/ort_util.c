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
