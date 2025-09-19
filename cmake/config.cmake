# CMake configuration options for Sieve of Eratosthenes project

# Build type options
# CMAKE_BUILD_TYPE can be: Debug, Release, RelWithDebInfo, MinSizeRel

# Platform detection
if(WIN32)
    set(PLATFORM "Windows")
    set(EXECUTABLE_SUFFIX ".exe")
elseif(UNIX AND NOT APPLE)
    set(PLATFORM "Linux")
    set(EXECUTABLE_SUFFIX "")
elseif(APPLE)
    set(PLATFORM "macOS")
    set(EXECUTABLE_SUFFIX "")
else()
    set(PLATFORM "Unknown")
    set(EXECUTABLE_SUFFIX "")
endif()

# Compiler-specific flags
if(CMAKE_C_COMPILER_ID MATCHES "GNU")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -fsanitize=address")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -DNDEBUG")
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -fsanitize=address")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -DNDEBUG")
elseif(CMAKE_C_COMPILER_ID MATCHES "MSVC")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /O2 /DNDEBUG")
endif()

# Feature detection
include(CheckIncludeFile)
include(CheckFunctionExists)
include(CheckSymbolExists)

# Check for required headers
check_include_file("unistd.h" HAVE_UNISTD_H)
check_include_file("sys/wait.h" HAVE_SYS_WAIT_H)
check_include_file("pthread.h" HAVE_PTHREAD_H)

# Check for required functions
if(HAVE_UNISTD_H)
    check_function_exists("fork" HAVE_FORK)
    check_function_exists("pipe" HAVE_PIPE)
    check_function_exists("mkfifo" HAVE_MKFIFO)
endif()

# Configure header file
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/config.h"
    @ONLY
)

# Include the configured header
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

# Print configuration summary
message(STATUS "=== Build Configuration ===")
message(STATUS "Platform: ${PLATFORM}")
message(STATUS "Compiler: ${CMAKE_C_COMPILER_ID}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C Standard: ${CMAKE_C_STANDARD}")
message(STATUS "")
message(STATUS "Feature Support:")
message(STATUS "  fork():    ${HAVE_FORK}")
message(STATUS "  pipe():    ${HAVE_PIPE}")
message(STATUS "  mkfifo():  ${HAVE_MKFIFO}")
message(STATUS "  pthread:   ${HAVE_PTHREAD_H}")
message(STATUS "===========================")