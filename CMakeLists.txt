cmake_minimum_required(VERSION 3.15)
project(myshell CXX)

set(CMAKE_CXX_STANDARD 17)

# Options
option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors." OFF)
option(ENABLE_SANITIZERS "Use sanitizers to detect errors." OFF)
option(ENABLE_CONAN "Use Conan as a package manager." ON)

# Include fixed CMake configuration
include(cmake/config.cmake)

# Project source compilation
include_directories(inc)
add_executable(mycat src/mycat/mycat.cpp src/mycat/operations_with_files.cpp inc/operations_with_files/operations_with_files.hpp)
add_executable(myshell src/main.cpp src/commands.cpp)
add_dependencies(myshell mycat)


# Add external packages
find_package(Boost 1.71.0 COMPONENTS program_options filesystem REQUIRED)
find_library(_READLINE_LIB readline)
if (NOT _READLINE_LIB)
    message(FATAL_ERROR "Not found libreadline.a")
endif ()
include_directories(${Boost_INCLUDE_DIR})
target_link_libraries(myshell Boost::program_options)
target_link_libraries(myshell Boost::filesystem)
target_link_libraries(mycat Boost::program_options)
target_link_libraries(myshell readline)
# Sanitizers configuration
if (ENABLE_SANITIZERS)
    set(SANITIZE_UNDEFINED ON)
    # Only one of Memory, Address, or Thread sanitizers is applicable at the time
    set(SANITIZE_MEMORY ON)
    #    set(SANITIZE_ADDRESS ON)
    #    set(SANITIZE_THREAD ON)
    find_package(Sanitizers)

    # CMake targets to which apply sanitizers
    #    add_sanitizers(operations add)
endif ()
