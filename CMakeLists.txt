cmake_minimum_required(VERSION 3.18)
project(EN605.617.81.FA21_StephenSeo_DitheringProject)

set(Project_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/image.cc
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")

if(NOT DEFINED CMAKE_BUILD_TYPE OR NOT CMAKE_BUILD_TYPE)
  message(WARNING "CMAKE_BUILD_TYPE not specified, setting to Debug...")
  set(CMAKE_BUILD_TYPE "Debug")
endif()

add_executable(DitheringProject
  ${Project_SOURCES})

find_package(PNG REQUIRED)

target_include_directories(DitheringProject PUBLIC
  ${PNG_INCLUDE_DIRS}
)
target_link_libraries(DitheringProject PUBLIC
  ${PNG_LIBRARIES}
)
