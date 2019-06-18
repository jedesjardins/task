cmake_minimum_required(VERSION 3.12)
project(task)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(TASK_BUILD_EXAMPLES "Build documentation examples" OFF)
option(TASK_BUILD_TESTS "Build documentation tests" OFF)

include(FetchContent)

FetchContent_Declare(
	Mul_Utl
	GIT_REPOSITORY "https://github.com/jedesjardins/mul_utl"
)

FetchContent_GetProperties(Mul_Utl)
if(NOT Mul_Utl_POPULATED)
	FetchContent_Populate(Mul_Utl)
	message(STATUS "Mul_Utl source dir: ${mul_utl_SOURCE_DIR}")
	message(STATUS "Mul_Utl binary dir: ${mul_utl_BINARY_DIR}")
	message(STATUS "Mul_Utl include dir: ${mul_utl_SOURCE_DIR}/include")
	add_subdirectory(${mul_utl_SOURCE_DIR} ${mul_utl_BINARY_DIR})
endif()

add_library(Task INTERFACE)
target_include_directories(Task
	INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
)

target_link_libraries(Task INTERFACE
	JED::mul_utl
)

add_library(JED::task ALIAS Task)

if(TASK_BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()

if(TASK_BUILD_TESTS)
	add_subdirectory(tests)
endif()