cmake_minimum_required(VERSION 3.5)
include(CheckIncludeFile)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(aprilasr)

cmake_policy(SET CMP0074 NEW)
if (DEFINED ENV{ONNX_ROOT})
  set(ONNXRuntime_ROOT_DIR $ENV{ONNX_ROOT})
else()
  set(ONNXRuntime_ROOT_DIR ${PROJECT_SOURCE_DIR}/lib)
endif()

find_package(ONNXRuntime REQUIRED)
if(NOT ONNXRuntime_FOUND)
  message(FATAL_ERROR "Could not find ONNXRuntime")
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
)

CHECK_INCLUDE_FILE(threads.h HAVE_C11_THREADS)
if(NOT HAVE_C11_THREADS)
    set(april_sources ${april_sources}
      src/tinycthread/tinycthread.c)
    
    add_compile_definitions(USE_TINYCTHREAD)
endif()

include_directories(${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src ${ORT_INCLUDE_DIR})

add_library(aprilasr_static STATIC ${april_sources})
target_link_libraries(aprilasr_static ONNXRuntime::ONNXRuntime)

add_library(aprilasr SHARED ${april_sources})
target_link_libraries(aprilasr ONNXRuntime::ONNXRuntime)

add_executable(main example.cpp)
target_link_libraries(main PRIVATE aprilasr_static ONNXRuntime::ONNXRuntime)