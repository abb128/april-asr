cmake_minimum_required(VERSION 3.5)
include(CheckIncludeFile)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(aprilasr)

if(WIN32)
    set(CMAKE_SHARED_LIBRARY_PREFIX "lib")
    set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
endif()

cmake_policy(SET CMP0074 NEW)
if (DEFINED ENV{ONNX_ROOT})
  set(ONNXRuntime_ROOT_DIR $ENV{ONNX_ROOT})
else()
  set(ONNXRuntime_ROOT_DIR ${PROJECT_SOURCE_DIR}/lib)
endif()

# To statically link, set environment variable like so (example):
#  ONNX_STATIC_ROOT=$HOME/Documents/onnxruntime/build/Linux/RelWithDebInfo
if (NOT DEFINED ENV{ONNX_STATIC_ROOT})
  find_package(ONNXRuntime REQUIRED)
  if(NOT ONNXRuntime_FOUND)
    message(FATAL_ERROR "Could not find ONNXRuntime")
  endif()

  set(onnx_link_libraries ONNXRuntime::ONNXRuntime)
else()
  set(onnx_link_libraries
    $ENV{ONNX_STATIC_ROOT}/libonnxruntime_session.a
    $ENV{ONNX_STATIC_ROOT}/libonnxruntime_optimizer.a
    $ENV{ONNX_STATIC_ROOT}/libonnxruntime_providers.a
    $ENV{ONNX_STATIC_ROOT}/libonnxruntime_framework.a
    $ENV{ONNX_STATIC_ROOT}/libonnxruntime_graph.a
    $ENV{ONNX_STATIC_ROOT}/libonnxruntime_util.a
    $ENV{ONNX_STATIC_ROOT}/libonnxruntime_mlas.a
    $ENV{ONNX_STATIC_ROOT}/libonnxruntime_common.a
    $ENV{ONNX_STATIC_ROOT}/libonnxruntime_flatbuffers.a
    $ENV{ONNX_STATIC_ROOT}/_deps/onnx-build/libonnx.a
    $ENV{ONNX_STATIC_ROOT}/_deps/onnx-build/libonnx_proto.a
    $ENV{ONNX_STATIC_ROOT}/_deps/protobuf-build/libprotobuf-lite.a
    $ENV{ONNX_STATIC_ROOT}/_deps/re2-build/libre2.a
    $ENV{ONNX_STATIC_ROOT}/_deps/flatbuffers-build/libflatbuffers.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/base/libabsl_base.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/base/libabsl_throw_delegate.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/container/libabsl_raw_hash_set.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/hash/libabsl_hash.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/hash/libabsl_city.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/hash/libabsl_low_level_hash.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/base/libabsl_raw_logging_internal.a
    $ENV{ONNX_STATIC_ROOT}/_deps/google_nsync-build/libnsync_cpp.a
    $ENV{ONNX_STATIC_ROOT}/_deps/pytorch_cpuinfo-build/libcpuinfo.a
    $ENV{ONNX_STATIC_ROOT}/_deps/pytorch_cpuinfo-build/deps/clog/libclog.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/strings/libabsl_cord.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/strings/libabsl_cordz_info.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/strings/libabsl_cord_internal.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/strings/libabsl_cordz_functions.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/strings/libabsl_cordz_handle.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/container/libabsl_raw_hash_set.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/container/libabsl_hashtablez_sampler.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/profiling/libabsl_exponential_biased.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/synchronization/libabsl_synchronization.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/synchronization/libabsl_graphcycles_internal.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/debugging/libabsl_stacktrace.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/debugging/libabsl_symbolize.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/base/libabsl_malloc_internal.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/debugging/libabsl_debugging_internal.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/debugging/libabsl_demangle_internal.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/time/libabsl_time.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/time/libabsl_civil_time.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/time/libabsl_time_zone.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/hash/libabsl_hash.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/types/libabsl_bad_optional_access.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/strings/libabsl_strings.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/base/libabsl_throw_delegate.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/strings/libabsl_strings_internal.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/types/libabsl_bad_variant_access.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/hash/libabsl_city.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/hash/libabsl_low_level_hash.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/base/libabsl_base.a
    $ENV{ONNX_STATIC_ROOT}/_deps/abseil_cpp-build/absl/base/libabsl_spinlock_wait.a
    dl
    rt
  )

  set(ORT_INCLUDE_DIR
    $ENV{ONNX_STATIC_ROOT}/../../../include/onnxruntime/core/session/
  )
endif()

if (DEFINED ENV{APRIL_DEBUG_SAVE_AUDIO})
    add_compile_definitions(APRIL_DEBUG_SAVE_AUDIO)
endif()

set(april_sources
  src/init.c
  src/april_model.c
  src/april_session.c
  src/audio_provider.c
  src/proc_thread.c
  src/params.c
  src/fbank.c
  src/ort_util.c
  src/file/model_file.c
  src/fft/pocketfft.c
  src/sonic/sonic.c
)

CHECK_INCLUDE_FILE(threads.h HAVE_C11_THREADS)
if((NOT HAVE_C11_THREADS) OR (DEFINED USE_TINYCTHREAD))
    set(april_sources ${april_sources}
      src/tinycthread/tinycthread.c)
    
    add_compile_definitions(USE_TINYCTHREAD)
endif()

include_directories(${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src ${ORT_INCLUDE_DIR})

add_library(aprilasr_static STATIC ${april_sources})
target_link_libraries(aprilasr_static ${onnx_link_libraries})

add_library(aprilasr SHARED ${april_sources})
target_link_libraries(aprilasr ${onnx_link_libraries})

add_executable(main example.cpp)
target_link_libraries(main PRIVATE aprilasr_static ${onnx_link_libraries})