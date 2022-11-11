# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.
#.rst:
# FindCUDNN
# -------
#
# Find CUDNN library
#
# Valiables that affect result:
# <VERSION>, <REQUIRED>, <QUIETLY>: as usual
#
# <EXACT> : as usual, plus we do find '5.1' version if you wanted '5'
#           (not if you wanted '5.0', as usual)
#
# Result target
# ^^^^^^^^^^^^^^^^
# CUDNN::cudnn
find_package(PkgConfig)
pkg_check_modules(PC_CUDNN QUIET CUDNN)

# We use major only in library search as major/minor is not entirely consistent among platforms.
# Also, looking for exact minor version of .so is in general not a good idea.
# More strict enforcement of minor/patch version is done if/when the header file is examined.
if(CUDNN_FIND_VERSION_EXACT)
  SET(__cudnn_ver_suffix ".${CUDNN_FIND_VERSION_MAJOR}")
  SET(__cudnn_lib_win_name cudnn64_${CUDNN_FIND_VERSION_MAJOR})
else()
  SET(__cudnn_lib_win_name cudnn64)
endif()
if(NOT CUDAToolkit_FOUND)
  find_package(CUDAToolkit)
endif()
find_library(CUDNN_LIBRARY 
  NAMES libcudnn.so${__cudnn_ver_suffix} libcudnn${__cudnn_ver_suffix}.dylib ${__cudnn_lib_win_name} cudnn.lib
  PATHS ${PC_CUDNN_LIBRARY_DIRS} ${CUDNN_ROOT} ${CUDAToolkit_ROOT} ${CUDAToolkit_LIBRARY_ROOT} $ENV{CUDA_ROOT} 
  PATH_SUFFIXES lib lib/x64 lib64 bin
  DOC "CUDNN library." )

if(CUDNN_LIBRARY)
  SET(CUDNN_MAJOR_VERSION ${CUDNN_FIND_VERSION_MAJOR})
  set(CUDNN_VERSION ${CUDNN_MAJOR_VERSION})
  get_filename_component(__found_cudnn_root ${CUDNN_LIBRARY} PATH)
  find_path(CUDNN_INCLUDE_DIR 
    NAMES "cudnn_version.h" "cudnn.h"
    HINTS ${PC_CUDNN_INCLUDE_DIRS} ${CUDNN_ROOT} ${CUDAToolkit_ROOT} ${CUDAToolkit_LIBRARY_ROOT} ${__found_cudnn_root}
    PATH_SUFFIXES include 
    DOC "Path to CUDNN include directory." )
  if(CUDNN_INCLUDE_DIR)
    if(EXISTS ${CUDNN_INCLUDE_DIR}/cudnn.h)
      file(READ ${CUDNN_INCLUDE_DIR}/cudnn.h CUDNN_VERSION_FILE_CONTENTS)
    else()
      file(READ ${CUDNN_INCLUDE_DIR}/cudnn_version.h CUDNN_VERSION_FILE_CONTENTS)
    endif()
    string(REGEX MATCH "define CUDNN_MAJOR * +([0-9]+)"
      CUDNN_MAJOR_VERSION "${CUDNN_VERSION_FILE_CONTENTS}")
    string(REGEX REPLACE "define CUDNN_MAJOR * +([0-9]+)" "\\1"
      CUDNN_MAJOR_VERSION "${CUDNN_MAJOR_VERSION}")
    string(REGEX MATCH "define CUDNN_MINOR * +([0-9]+)"
      CUDNN_MINOR_VERSION "${CUDNN_VERSION_FILE_CONTENTS}")
    string(REGEX REPLACE "define CUDNN_MINOR * +([0-9]+)" "\\1"
      CUDNN_MINOR_VERSION "${CUDNN_MINOR_VERSION}")
    string(REGEX MATCH "define CUDNN_PATCHLEVEL * +([0-9]+)"
      CUDNN_PATCH_VERSION "${CUDNN_VERSION_FILE_CONTENTS}")
    string(REGEX REPLACE "define CUDNN_PATCHLEVEL * +([0-9]+)" "\\1"
      CUDNN_PATCH_VERSION "${CUDNN_PATCH_VERSION}")  
    set(CUDNN_VERSION ${CUDNN_MAJOR_VERSION}.${CUDNN_MINOR_VERSION})
    if(CUDNN_MAJOR_VERSION)
      ## Fixing the case where 5.1 does not fit 'exact' 5.
      if(CUDNN_FIND_VERSION_EXACT AND NOT CUDNN_FIND_VERSION_MINOR)
        if("${CUDNN_MAJOR_VERSION}" STREQUAL "${CUDNN_FIND_VERSION_MAJOR}")
          set(CUDNN_VERSION ${CUDNN_FIND_VERSION})
        endif()
      endif()
    else()
      # Try to set CUDNN version from config file
      set(CUDNN_VERSION ${PC_CUDNN_CFLAGS_OTHER})
    endif()
    if(NOT TARGET CUDNN::cudnn)
      add_library(CUDNN::cudnn SHARED IMPORTED)
      set_target_properties(CUDNN::cudnn PROPERTIES 
        INTERFACE_INCLUDE_DIRECTORIES ${CUDNN_INCLUDE_DIR}
      )
      if(WIN32)
        set_target_properties(CUDNN::cudnn PROPERTIES
          IMPORTED_IMPLIB ${CUDNN_LIBRARY}
          IMPORTED_LOCATION ${__found_cudnn_root}cudnn64_${CUDNN_MAJOR_VERSION}.dll
        )
      else()
        set_target_properties(CUDNN::cudnn PROPERTIES IMPORTED_LOCATION ${CUDNN_LIBRARY})
      endif()
    endif()
  endif()
endif()

find_package_handle_standard_args(
  CUDNN 
  FOUND_VAR CUDNN_FOUND
  REQUIRED_VARS CUDNN_LIBRARY
  VERSION_VAR CUDNN_VERSION
  )