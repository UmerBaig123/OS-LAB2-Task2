cmake_minimum_required(VERSION 3.10)
project(SieveOfEratosthenes VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Include configuration
include(cmake/config.cmake)

# Set compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2")

# Debug flags
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -DDEBUG")

# Check for pthread support
find_package(Threads REQUIRED)

# Platform-specific settings
if(WIN32)
    message(STATUS "Building for Windows")
    set(PLATFORM_LIBS "")
    # Only build iterative and pthread versions on Windows
    set(BUILD_UNIX_VERSIONS OFF)
else()
    message(STATUS "Building for Unix/Linux")
    set(PLATFORM_LIBS "")
    set(BUILD_UNIX_VERSIONS ON)
endif()

# Executable: Iterative Implementation
add_executable(sieve_iterative sieve_iterative.c)
target_compile_definitions(sieve_iterative PRIVATE)

# Executable: Pthread Implementation
add_executable(sieve_pthread sieve_pthread.c)
target_link_libraries(sieve_pthread Threads::Threads)

# Unix/Linux only executables
if(BUILD_UNIX_VERSIONS)
    # Executable: Fork and Anonymous Pipe Implementation
    add_executable(sieve_fork_pipe sieve_fork_pipe.c)
    target_compile_definitions(sieve_fork_pipe PRIVATE _GNU_SOURCE)
    
    # Executable: Fork and Named Pipe Implementation
    add_executable(sieve_named_pipe sieve_named_pipe.c)
    target_compile_definitions(sieve_named_pipe PRIVATE _GNU_SOURCE)
    
    message(STATUS "Unix/Linux versions will be built")
else()
    message(STATUS "Skipping Unix/Linux-specific implementations on Windows")
endif()

# Custom target to build all available implementations
if(BUILD_UNIX_VERSIONS)
    add_custom_target(all_sieves 
        DEPENDS sieve_iterative sieve_pthread sieve_fork_pipe sieve_named_pipe
        COMMENT "Building all sieve implementations"
    )
else()
    add_custom_target(all_sieves 
        DEPENDS sieve_iterative sieve_pthread
        COMMENT "Building Windows-compatible sieve implementations"
    )
endif()

# Installation
install(TARGETS sieve_iterative sieve_pthread 
        RUNTIME DESTINATION bin)

if(BUILD_UNIX_VERSIONS)
    install(TARGETS sieve_fork_pipe sieve_named_pipe
            RUNTIME DESTINATION bin)
endif()

# Documentation
install(FILES README_new.md DESTINATION doc RENAME README.md)

# Print build information
message(STATUS "CMake build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C Flags: ${CMAKE_C_FLAGS}")

# Custom targets for running each implementation
add_custom_target(run_iterative
    COMMAND sieve_iterative
    DEPENDS sieve_iterative
    COMMENT "Running iterative sieve implementation"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(run_pthread
    COMMAND sieve_pthread
    DEPENDS sieve_pthread
    COMMENT "Running pthread sieve implementation"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

if(BUILD_UNIX_VERSIONS)
    add_custom_target(run_fork_pipe
        COMMAND sieve_fork_pipe
        DEPENDS sieve_fork_pipe
        COMMENT "Running fork/pipe sieve implementation"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    
    add_custom_target(run_named_pipe
        COMMAND sieve_named_pipe
        DEPENDS sieve_named_pipe
        COMMENT "Running named pipe sieve implementation"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()

# Test target to run all implementations
if(BUILD_UNIX_VERSIONS)
    add_custom_target(test_all
        COMMAND echo "Running iterative implementation:" && sieve_iterative && echo ""
        COMMAND echo "Running pthread implementation:" && sieve_pthread && echo ""
        COMMAND echo "Running fork/pipe implementation:" && sieve_fork_pipe && echo ""
        COMMAND echo "Running named pipe implementation:" && sieve_named_pipe
        DEPENDS sieve_iterative sieve_pthread sieve_fork_pipe sieve_named_pipe
        COMMENT "Running all sieve implementations"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
else()
    add_custom_target(test_all
        COMMAND echo "Running iterative implementation:" && sieve_iterative && echo ""
        COMMAND echo "Running pthread implementation:" && sieve_pthread
        DEPENDS sieve_iterative sieve_pthread
        COMMENT "Running Windows-compatible sieve implementations"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()

# Package configuration
set(CPACK_PACKAGE_NAME "SieveOfEratosthenes")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Four different implementations of Sieve of Eratosthenes")
set(CPACK_PACKAGE_VENDOR "OS Course Project")

include(CPack)