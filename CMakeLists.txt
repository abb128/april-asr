cmake_minimum_required(VERSION 3.5)
include(CheckIncludeFile)
include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(april-asr
        VERSION 2023.05.12
        DESCRIPTION "aprilasr is a minimal library that provides an API for offline streaming speech-to-text applications"
        HOMEPAGE_URL "https://github.com/abb128/april-asr")

string(REPLACE "-" "" PROJECT_NAME_SO ${PROJECT_NAME})

add_subdirectory(src/ggml)

cmake_policy(SET CMP0074 NEW)

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
  src/file/model_file.c
  src/fft/pocketfft.c
  src/sonic/sonic.c
  src/model_impl.cpp
)

file(GLOB_RECURSE april_headers "*.h")

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

include_directories(${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/src/ggml/include)

add_library(aprilasr_static STATIC ${april_sources})
target_link_libraries(aprilasr_static ggml ${april_link_libraries})

add_library(aprilasr SHARED ${april_sources})
target_link_libraries(aprilasr ggml ${april_link_libraries})

set_target_properties(aprilasr PROPERTIES VERSION ${CMAKE_PROJECT_VERSION}
SOVERSION ${PROJECT_VERSION_MAJOR} )
set_target_properties(aprilasr PROPERTIES PUBLIC_HEADER "${april_headers}")

add_executable(main example.cpp)
target_link_libraries(main PRIVATE aprilasr_static ${april_link_libraries})

add_executable(srt example_srt.cpp)
target_link_libraries(srt PRIVATE aprilasr_static ${april_link_libraries})

add_executable(stress rt_stress.cpp)
target_link_libraries(stress PRIVATE aprilasr_static ${april_link_libraries})

add_executable(quantize src/tools/quantize.cpp)
target_link_libraries(quantize PRIVATE aprilasr_static ${april_link_libraries})

install(TARGETS aprilasr
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.pc.in ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc
  @ONLY
)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc
  DESTINATION lib/pkgconfig
)
