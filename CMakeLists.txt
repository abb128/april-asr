cmake_minimum_required(VERSION 3.5)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(aprilasr)

cmake_policy(SET CMP0074 NEW)
set(onnxruntime_ROOT ${PROJECT_SOURCE_DIR}/lib)
find_package(onnxruntime REQUIRED)
if(NOT onnxruntime_FOUND)
  message(FATAL_ERROR "Could not find onnxruntime")
endif()


include_directories(${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/fft ${ORT_INCLUDE_DIR})

add_library(aprilasr april.c fft/pocketfft.c fft/fbank.c)
add_executable(main main.cpp)

target_link_libraries(main PRIVATE aprilasr onnxruntime::onnxruntime)
