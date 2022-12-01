cmake_minimum_required(VERSION 3.5)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(aprilasr)

cmake_policy(SET CMP0074 NEW)
if (DEFINED ENV{FLATPAK_DEST})
  set(onnxruntime_ROOT $ENV{FLATPAK_DEST}/onnxruntime_ROOT)
else()
  set(onnxruntime_ROOT ${PROJECT_SOURCE_DIR}/lib)
endif()

find_package(onnxruntime REQUIRED)
if(NOT onnxruntime_FOUND)
  message(FATAL_ERROR "Could not find onnxruntime")
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

target_link_libraries(main PRIVATE aprilasr_static onnxruntime::onnxruntime)

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

target_link_libraries(aprilasr_static m dl onnxruntime::onnxruntime)
target_link_libraries(aprilasr m dl onnxruntime::onnxruntime)