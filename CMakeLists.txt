cmake_minimum_required(VERSION 3.5)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(aprilasr)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__FILENAME__='\"$(subst ${CMAKE_SOURCE_DIR}/,,$(abspath $<))\"'")

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


include_directories(${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src ${ORT_INCLUDE_DIR})

add_library(aprilasr_static
  STATIC
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

add_executable(main example.cpp)

target_link_libraries(main PRIVATE aprilasr_static ONNXRuntime::ONNXRuntime)

add_library(aprilasr
  SHARED
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

target_link_libraries(aprilasr_static m dl ONNXRuntime::ONNXRuntime)
target_link_libraries(aprilasr m dl ONNXRuntime::ONNXRuntime)