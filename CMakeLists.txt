
cmake_minimum_required(VERSION 3.16)
project(mini_allocator CXX)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include headers
include_directories(${PROJECT_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/allocator.cpp
    tests/test_allocator.cpp
)

# Add executable
add_executable(test_allocator ${SOURCES})

# Compiler warnings and optimization
if(MSVC)
    target_compile_options(test_allocator PRIVATE /W4 /O2)
else()
    target_compile_options(test_allocator PRIVATE -Wall -Wextra -O2)
endif()
