find_path(TensorRT_INCLUDE_DIR NvInfer.h
  HINTS ${TensorRT_ROOT} ${CUDA_TOOLKIT_ROOT_DIR}
  PATH_SUFFIXES include)
find_library(TensorRT_LIBRARY_INFER nvinfer
  HINTS ${TensorRT_ROOT} ${TensorRT_BUILD} ${CUDA_TOOLKIT_ROOT_DIR} /usr/
  PATH_SUFFIXES lib lib64 lib/x64 lib/x86_64-linux-gnu/)
find_library(TensorRT_LIBRARY_INFER_PLUGIN nvinfer_plugin
  HINTS  ${TensorRT_ROOT} ${TensorRT_BUILD} ${CUDA_TOOLKIT_ROOT_DIR} /usr/
  PATH_SUFFIXES lib lib64 lib/x64 lib/x86_64-linux-gnu/)

mark_as_advanced(TensorRT_INCLUDE_DIR)

if(TensorRT_INCLUDE_DIR AND EXISTS "${TensorRT_INCLUDE_DIR}/NvInferVersion.h")
  file(STRINGS "${TensorRT_INCLUDE_DIR}/NvInferVersion.h" TensorRT_MAJOR REGEX "^#define NV_TENSORRT_MAJOR [0-9]+.*$")
  file(STRINGS "${TensorRT_INCLUDE_DIR}/NvInferVersion.h" TensorRT_MINOR REGEX "^#define NV_TENSORRT_MINOR [0-9]+.*$")
  file(STRINGS "${TensorRT_INCLUDE_DIR}/NvInferVersion.h" TensorRT_PATCH REGEX "^#define NV_TENSORRT_PATCH [0-9]+.*$")

  string(REGEX REPLACE "^#define NV_TENSORRT_MAJOR ([0-9]+).*$" "\\1" TensorRT_VERSION_MAJOR "${TensorRT_MAJOR}")
  string(REGEX REPLACE "^#define NV_TENSORRT_MINOR ([0-9]+).*$" "\\1" TensorRT_VERSION_MINOR "${TensorRT_MINOR}")
  string(REGEX REPLACE "^#define NV_TENSORRT_PATCH ([0-9]+).*$" "\\1" TensorRT_VERSION_PATCH "${TensorRT_PATCH}")
  set(TensorRT_VERSION_STRING "${TensorRT_VERSION_MAJOR}.${TensorRT_VERSION_MINOR}.${TensorRT_VERSION_PATCH}")
endif()

if(TensorRT_FOUND)
  set(TensorRT_INCLUDE_DIRS ${TensorRT_INCLUDE_DIR})

  if(NOT TensorRT_LIBRARIES)
    set(TensorRT_LIBRARIES ${TensorRT_LIBRARY_INFER} ${TensorRT_LIBRARY_INFER_PLUGIN})
  endif()

  if(NOT TARGET TensorRT::TensorRT)
    add_library(TensorRT::TensorRT UNKNOWN IMPORTED)
    set_target_properties(TensorRT::TensorRT PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${TensorRT_INCLUDE_DIRS}")
    set_property(TARGET TensorRT::TensorRT APPEND PROPERTY IMPORTED_LOCATION "${TensorRT_LIBRARY_INFER}")
  endif()
  if(NOT TARGET TensorRT::plugin)
    add_library(TensorRT::plugin UNKNOWN IMPORTED)
    set_target_properties(TensorRT::plugin PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${TensorRT_INCLUDE_DIRS}")
    set_property(TARGET TensorRT::plugin APPEND PROPERTY IMPORTED_LOCATION "${TensorRT_LIBRARY_INFER_PLUGIN}")
  endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  TensorRT 
  REQUIRED_VARS TensorRT_INCLUDE_DIR TensorRT_LIBRARY_INFER TensorRT_LIBRARY_INFER_PLUGIN
  FOUND_VAR TensorRT_FOUND
  VERSION_VAR TensorRT_VERSION_STRING
  HANDLE_COMPONENTS)