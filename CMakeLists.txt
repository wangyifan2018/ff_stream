cmake_minimum_required (VERSION 2.8.11)
project (ff_stream_push)
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR})

if (NOT DEFINED TARGET_ARCH)
    set(TARGET_ARCH pcie)
endif()

if (${TARGET_ARCH} STREQUAL "soc")
    set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
    set(CMAKE_ASM_COMPILER aarch64-linux-gnu-gcc)
    set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

elseif (${TARGET_ARCH} STREQUAL "pcie")
    set(CMAKE_C_COMPILER gcc)
    set(CMAKE_ASM_COMPILER gcc)
    set(CMAKE_CXX_COMPILER g++)
else()
     message( FATAL_ERROR  "not support mode, eg: soc/pcie, cur: ${TARGET_ARCH}")
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_CFLAGS} -std=c++11")

set(FFMPEG_DIR  /opt/sophon/sophon-ffmpeg-latest/lib/cmake)
find_package(FFMPEG REQUIRED)
include_directories(${FFMPEG_INCLUDE_DIRS})
link_directories(${FFMPEG_LIB_DIRS})

# use opencv
set(OpenCV_DIR  /opt/sophon/sophon-opencv-latest/lib/cmake/opencv4)
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})
link_directories(${OpenCV_LIB_DIRS})

# use libbmrt libbmlib
find_package(libsophon REQUIRED)
include_directories(${LIBSOPHON_INCLUDE_DIRS})
link_directories(${LIBSOPHON_LIB_DIRS})

include_directories(PROJECT_SOURCE_DIR)

aux_source_directory(. SRC_FILES)
add_executable(ff_stream_push ${SRC_FILES})
target_link_libraries(ff_stream_push ${FFMPEG_LIBS} ${OpenCV_LIBS} -lpthread)

