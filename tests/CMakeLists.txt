cmake_minimum_required(VERSION 3.12)
project(task_tests)

message(STATUS "Tests included")

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

# gather all test files
set(SOURCES
	# list test files here
	test_main.cpp
)

add_executable(tests ${SOURCES})
target_link_libraries(tests Task::Task Catch2::Catch2)
