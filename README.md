# Task

A single header library providing a multithreaded job based system

# How to use it:

[Reference section](docs/Readme.md) - all the details

# Quick Usage:

You can just drop the single header file at include/task.hpp into your include path and use it from there.

The implementation doesn't use inline functions, instead uses a preprocessor conditional guard to stop it from being included. Simply add a #define JED_TASK_IMPLEMENTATION in a single file before your include to build the implementation.



