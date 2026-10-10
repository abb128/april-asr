cmake_minimum_required(VERSION 3.16...4.4)

if(POLICY CMP0126)
  cmake_policy(SET CMP0126 NEW)
endif()
if(POLICY CMP0156)
  cmake_policy(SET CMP0156 NEW)
endif()
if(POLICY CMP0181)
  cmake_policy(SET CMP0181 NEW)
endif()
if(POLICY CMP0200)
  cmake_policy(SET CMP0200 NEW)
endif()

include(CheckIncludeFile)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(april-asr
        VERSION 2023.05.12
        DESCRIPTION "aprilasr is a minimal library that provides an API for offline streaming speech-to-text applications"
        HOMEPAGE_URL "https://github.com/abb128/april-asr")
include(GNUInstallDirs)
include(CTest)
option(APRIL_NATIVE "Optimize for the CPU used to build (not portable)" OFF)
option(APRIL_LTO "Enable link-time optimization" OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(APRIL_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT april_ipo_supported OUTPUT april_ipo_error)
  if(NOT april_ipo_supported)
    message(FATAL_ERROR "LTO unavailable: ${april_ipo_error}")
  endif()
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

string(REPLACE "-" "" PROJECT_NAME_SO ${PROJECT_NAME})

if(WIN32)
    set(CMAKE_SHARED_LIBRARY_PREFIX "lib")
    set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
endif()

cmake_policy(SET CMP0074 NEW)
if (DEFINED ENV{ONNX_ROOT})
  set(ONNXRuntime_ROOT_DIR $ENV{ONNX_ROOT})
elseif(NOT DEFINED ONNXRuntime_ROOT_DIR)
  set(ONNXRuntime_ROOT_DIR ${PROJECT_SOURCE_DIR}/lib)
endif()

# To statically link, set environment variable like so (example):
#  ONNX_STATIC_ROOT=$HOME/Documents/onnxruntime/build/Linux/RelWithDebInfo
if (NOT DEFINED ENV{ONNX_STATIC_ROOT})
  find_package(ONNXRuntime REQUIRED)
  if(NOT ONNXRuntime_FOUND)
    message(FATAL_ERROR "Could not find ONNXRuntime")
  endif()

  set(april_link_libraries ONNXRuntime::ONNXRuntime)

  # A downloaded SDK is not on the loader's search path. Record its directory
  # in programs linking aprilasr_static (such as Live Captions) so they run
  # without LD_LIBRARY_PATH. System copies need no runtime path.
  get_filename_component(april_ort_dir "${ONNXRuntime_LIBRARY}" DIRECTORY)
  file(REAL_PATH "${ONNXRuntime_ROOT_DIR}" april_ort_root)
  file(REAL_PATH "${april_ort_dir}" april_ort_dir)
  string(FIND "${april_ort_dir}/" "${april_ort_root}/" april_ort_in_root)
  if(NOT WIN32 AND NOT APPLE AND april_ort_in_root EQUAL 0)
    set(april_ort_rpath "${april_ort_dir}")
  endif()
else()
  set(april_link_libraries
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

set(april_headers "${PROJECT_SOURCE_DIR}/april_api.h")

if(DEFINED USE_TINYCTHREAD)
    set(HAVE_C11_THREADS FALSE)
else()
    CHECK_INCLUDE_FILE(threads.h HAVE_C11_THREADS)
endif()

if((NOT HAVE_C11_THREADS) OR (DEFINED USE_TINYCTHREAD))
    set(april_sources ${april_sources}
      src/tinycthread/tinycthread.c)
    
    add_compile_definitions(USE_TINYCTHREAD)
endif()

if(NOT WIN32)
  list(APPEND april_link_libraries "pthread")
  list(APPEND april_link_libraries "m")
endif()

include_directories(${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src ${ORT_INCLUDE_DIR})

set_source_files_properties(
  src/sonic/sonic.c
  PROPERTIES
  COMPILE_OPTIONS "-Wno-unused-parameter"
)

add_library(aprilasr_static STATIC ${april_sources})
target_link_libraries(aprilasr_static ${april_link_libraries})
if(april_ort_rpath)
  target_link_options(aprilasr_static INTERFACE "-Wl,-rpath,${april_ort_rpath}")
endif()

add_library(aprilasr SHARED ${april_sources})
target_link_libraries(aprilasr ${april_link_libraries})

set_target_properties(aprilasr PROPERTIES VERSION ${CMAKE_PROJECT_VERSION}
SOVERSION ${PROJECT_VERSION_MAJOR} )
set_target_properties(aprilasr PROPERTIES PUBLIC_HEADER "${april_headers}")
if(APRIL_NATIVE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(aprilasr PRIVATE -march=native)
  target_compile_options(aprilasr_static PRIVATE -march=native)
endif()

add_executable(main example.cpp)
target_link_libraries(main PRIVATE aprilasr_static ${april_link_libraries})

add_executable(srt example_srt.cpp)
target_link_libraries(srt PRIVATE aprilasr_static ${april_link_libraries})

if(BUILD_TESTING)
  foreach(april_test test_thread test_audio test_fbank test_model)
    add_executable(${april_test} tests/${april_test}.c)
    target_link_libraries(${april_test} PRIVATE aprilasr_static)
    if(MSVC)
      target_compile_options(${april_test} PRIVATE /UNDEBUG)
    else()
      target_compile_options(${april_test} PRIVATE -UNDEBUG)
    endif()
    add_test(NAME ${april_test} COMMAND ${april_test} "${CMAKE_CURRENT_BINARY_DIR}/${april_test}-fixture.april")
    set_tests_properties(${april_test} PROPERTIES TIMEOUT 30 ENVIRONMENT "ORT_DISABLE_TELEMETRY=1")
  endforeach()
endif()

install(TARGETS aprilasr
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.pc.in ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc
  @ONLY
)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
