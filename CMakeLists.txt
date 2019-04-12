cmake_minimum_required(VERSION 3.12)
project(task)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(TASK_BUILD_EXAMPLES "Build documentation examples" OFF)
option(TASK_BUILD_TESTS "Build documentation tests" OFF)

add_library(Task INTERFACE)
target_include_directories(Task
	INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
)

add_library(Task::Task ALIAS Task)

if(TASK_BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()

if(TASK_BUILD_TESTS)
	add_subdirectory(tests)
endif()