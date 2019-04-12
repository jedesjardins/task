cmake_minimum_required(VERSION 3.12)
project(task)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
	Catch2
	GIT_REPOSITORY "https://github.com/catchorg/Catch2"
)

FetchContent_GetProperties(Catch2)
if(NOT Catch2_POPULATED)
	FetchContent_Populate(Catch2)
	message(STATUS "Catch source dir: ${catch2_SOURCE_DIR}")
	message(STATUS "Catch binary dir: ${catch2_BINARY_DIR}")
	message(STATUS "Catch include dir: ${catch2_SOURCE_DIR}/include")
	add_subdirectory(${catch2_SOURCE_DIR} ${catch2_BINARY_DIR})
endif()


add_library(Task INTERFACE)
target_include_directories(Task
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
)

add_subdirectory(examples)