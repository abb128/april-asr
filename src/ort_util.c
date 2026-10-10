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

#include <string.h>
#include "common.h"
#include "ort_util.h"

bool tensor_info(OrtSession *session, bool output, size_t index, const char *name,
                 ONNXTensorElementDataType type, int64_t *dimensions, size_t rank) {
    OrtTypeInfo *info = NULL;
    const OrtTensorTypeAndShapeInfo *tensor = NULL;
    OrtAllocator *allocator = NULL;
    char *actual_name = NULL;
    size_t actual_rank = 0;
    ONNXTensorElementDataType actual_type;
    bool ok = false;
    if (!ort_ok(g_ort->GetAllocatorWithDefaultOptions(&allocator))) goto done;
    if (!ort_ok(output ? g_ort->SessionGetOutputName(session, index, allocator, &actual_name)
                       : g_ort->SessionGetInputName(session, index, allocator, &actual_name))) goto done;
    if (!actual_name || strcmp(actual_name, name)) goto done;
    if (!ort_ok(output ? g_ort->SessionGetOutputTypeInfo(session, index, &info)
                       : g_ort->SessionGetInputTypeInfo(session, index, &info))) goto done;
    if (!ort_ok(g_ort->CastTypeInfoToTensorInfo(info, &tensor)) || !tensor) goto done;
    if (!ort_ok(g_ort->GetTensorElementType(tensor, &actual_type)) || actual_type != type) goto done;
    if (!ort_ok(g_ort->GetDimensionsCount(tensor, &actual_rank)) || actual_rank != rank) goto done;
    if (!ort_ok(g_ort->GetDimensions(tensor, dimensions, rank))) goto done;
    size_t count = 1;
    for (size_t i = 0; i < rank; ++i) {
        if (dimensions[i] <= 0 || (uint64_t)dimensions[i] > SIZE_MAX / sizeof(int64_t) / count) goto done;
        count *= (size_t)dimensions[i];
    }
    ok = true;
done:
    if (actual_name) allocator->Free(allocator, actual_name);
    if (info) g_ort->ReleaseTypeInfo(info);
    if (!ok) LOG_ERROR("Unsupported tensor contract for %s", name);
    return ok;
}
