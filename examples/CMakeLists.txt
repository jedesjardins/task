cmake_minimum_required(VERSION 3.12)
project(task_examples)

message(STATUS "Examples included")

# Modeled after Catch2 examples directory

# gather all example files
set(SOURCES_SINGLE_FILE
	# list test files here
	simple.cpp
	store_data.cpp
	store_pointer_to_data.cpp
	store_pointer_to_array.cpp
	parallel_for.cpp
	children_tasks.cpp
)

# get base names of each file (strips off .cpp)
string(REPLACE ".cpp" "" BASENAMES_SINGLE_FILE "${SOURCES_SINGLE_FILE}")

# gather list of single file targets
set(TARGETS_SINGLE_FILE ${BASENAMES_SINGLE_FILE} )

foreach(name ${TARGETS_SINGLE_FILE})
    add_executable(${name} ${CMAKE_CURRENT_LIST_DIR}/${name}.cpp)
    target_link_libraries(${name} JED::task)
endforeach()