cmake_minimum_required(VERSION 3.13)
project(EN605.617.81.FA21_StephenSeo_DitheringProject)

set(Project_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/arg_parse.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/image.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/video.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/opencl_handle.cc
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")

if(NOT DEFINED CMAKE_BUILD_TYPE OR NOT CMAKE_BUILD_TYPE)
  message(WARNING "CMAKE_BUILD_TYPE not specified, setting to Debug...")
  set(CMAKE_BUILD_TYPE "Debug")
endif()

add_executable(DitheringProject
  ${Project_SOURCES})
#target_compile_features(DitheringProject PUBLIC cxx_std_11)

find_package(OpenCL REQUIRED)

find_package(PNG REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG_LIBAVCODEC REQUIRED
    libavcodec libavformat libavutil libswscale)

if(${FFMPEG_LIBAVCODEC_libavcodec_VERSION} VERSION_GREATER "58.134.100"
        OR
    ${FFMPEG_LIBAVCODEC_libavformat_VERSION} VERSION_GREATER "58.76.100"
        OR
    ${FFMPEG_LIBAVCODEC_libavutil_VERSION} VERSION_GREATER "56.70.100"
        OR
    ${FFMPEG_LIBAVCODEC_libswscale_VERSION} VERSION_GREATER "5.9.100")
    message(FATAL_ERROR "FFmpeg version is newer than expected, please use FFmpeg version 4.4")
# The following may be necessary when moving to FFmpeg 5.0
#elseif(${FFMPEG_LIBAVCODEC_libavcodec_VERSION} VERSION_LESS_EQUAL "58.91.100"
#        OR
#    ${FFMPEG_LIBAVCODEC_libavformat_VERSION} VERSION_LESS_EQUAL "58.45.100"
#        OR
#    ${FFMPEG_LIBAVCODEC_libavutil_VERSION} VERSION_LESS_EQUAL "56.51.100"
#        OR
#    ${FFMPEG_LIBAVCODEC_libswscale_VERSION} VERSION_LESS_EQUAL "5.7.100")
#    message(FATAL_ERROR "FFmpeg version is older than expected, please use FFmpeg version 4.4")
else()
    message("FFmpeg version is OK")
endif()

target_include_directories(DitheringProject PUBLIC
  ${OpenCL_INCLUDE_DIRS}
  ${PNG_INCLUDE_DIRS}
  ${FFMPEG_LIBAVCODEC_INCLUDE_DIRS}
)
target_link_libraries(DitheringProject PUBLIC
  ${OpenCL_LIBRARIES}
  ${PNG_LIBRARIES}
  ${FFMPEG_LIBAVCODEC_LINK_LIBRARIES}
)
