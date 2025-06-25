cmake_minimum_required(VERSION 3.14)
project(sta_taskflow VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra -pthread")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG_TIMING")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Find Taskflow
set(TASKFLOW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/taskflow)
if(NOT EXISTS ${TASKFLOW_DIR})
    message(FATAL_ERROR "Taskflow not found. Please run: git clone https://github.com/taskflow/taskflow.git external/taskflow")
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${TASKFLOW_DIR}
)

# Source files
set(SOURCES
    src/main.cpp
    src/Circuit.cpp
    src/CircuitNode.cpp
    src/GateDatabase.cpp
    src/TaskflowTimingEngine.cpp
    src/TimingUtils.cpp
    src/Logger.cpp
)

# Create executable
add_executable(sta ${SOURCES})

# Link libraries
target_link_libraries(sta pthread)

# Enable Taskflow profiler support
target_compile_definitions(sta PRIVATE TF_ENABLE_PROFILER)

# Copy test files to build directory
file(COPY ../ref/test DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Create reference executable for validation (optional)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../ref)
    add_custom_target(validate
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/validate_taskflow.sh
        DEPENDS sta
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_target(performance
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/measure_performance.sh
        DEPENDS sta
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endif() 