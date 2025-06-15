# Taskflow STA Implementation Setup Guide for Cursor

## 1. System Requirements

### 1.1. Compiler Requirements
Based on Taskflow's requirements, ensure one of the following compilers is installed:

- **GNU C++ Compiler**: g++ version 8.4 or later
- **Clang**: version 6.0 or later  
- **MSVC**: version 19.14 or later (Visual Studio 2019)
- **AppleClang**: Xcode 12.0 or later

### 1.2. C++ Standard
- Minimum: C++17 (`-std=c++17`)
- Recommended: C++20 (`-std=c++20`) for better performance

### 1.3. Operating System
- Linux (Ubuntu 18.04 or later recommended)
- macOS (10.14 or later)
- Windows (with MSVC 2019 or later)

## 2. Project Structure Setup

Create the following directory structure:

```
sta_taskflow/
├── CMakeLists.txt
├── README.md
├── include/
│   ├── Circuit.hpp
│   ├── CircuitNode.hpp
│   ├── GateDatabase.hpp
│   ├── TaskflowTimingEngine.hpp
│   └── TimingUtils.hpp
├── src/
│   ├── main.cpp
│   ├── Circuit.cpp
│   ├── CircuitNode.cpp
│   ├── GateDatabase.cpp
│   ├── TaskflowTimingEngine.cpp
│   └── TimingUtils.cpp
├── test/
│   ├── NLDM_lib_max2Inp
│   └── cleaned_iscas89_99_circuits/
│       ├── c17.isc
│       ├── c432.isc
│       └── ... (other test circuits)
├── scripts/
│   ├── validate_taskflow.sh
│   ├── measure_performance.sh
│   └── run_all_tests.sh
├── external/
│   └── taskflow/  (will be cloned here)
└── build/          (created by CMake)
```

## 3. Installing Dependencies

### 3.1. Clone Taskflow

```bash
# From project root directory
mkdir -p external
cd external
git clone https://github.com/taskflow/taskflow.git
cd ..
```

### 3.2. Install CMake (if not already installed)

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install cmake build-essential
```

**macOS (using Homebrew):**
```bash
brew install cmake
```

**Windows:**
Download and install from https://cmake.org/download/

### 3.3. Install Additional Tools (Optional but Recommended)

**For visualization and debugging:**
```bash
# Ubuntu/Debian
sudo apt install graphviz

# macOS
brew install graphviz

# For performance profiling
sudo apt install linux-tools-common linux-tools-generic
```

## 4. CMakeLists.txt Configuration

Create `CMakeLists.txt` in the project root:

```cmake
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
)

# Create executable
add_executable(sta ${SOURCES})

# Link libraries
target_link_libraries(sta pthread)

# Enable Taskflow profiler support
target_compile_definitions(sta PRIVATE TF_ENABLE_PROFILER)

# Copy test files to build directory
file(COPY test DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Create reference executable for validation (optional)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../ref)
    add_executable(sta_ref
        ../ref/src/main.cpp
        ../ref/src/Circuit.cpp
        ../ref/src/CircuitNode.cpp
        ../ref/src/GateDatabase.cpp
    )
    target_include_directories(sta_ref PRIVATE ../ref/include)
endif()

# Add custom targets for testing
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
```

## 5. Build Instructions

### 5.1. Standard Build Process

```bash
# From project root
mkdir -p build
cd build
cmake ..
make -j$(nproc)  # Use all available cores
```

### 5.2. Debug Build

```bash
mkdir -p build_debug
cd build_debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

### 5.3. Build with Specific Compiler

```bash
# Using clang++
CC=clang CXX=clang++ cmake ..

# Using g++-11
CC=gcc-11 CXX=g++-11 cmake ..
```

## 6. Essential Code Templates for Cursor

### 6.1. Include Taskflow Header

At the top of `TaskflowTimingEngine.cpp`:

```cpp
// Taskflow is header-only, just include it
#include <taskflow/taskflow.hpp>

// For floating-point control
#include <cfenv>
#include <cmath>

// For threading
#include <thread>
#include <atomic>

// Standard library
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
```

### 6.2. Taskflow Initialization Template

```cpp
// In TaskflowTimingEngine constructor
TaskflowTimingEngine::TaskflowTimingEngine(Circuit& ckt) 
    : circuit(ckt), 
      executor(std::thread::hardware_concurrency()) {
    // Executor will use all available CPU cores
    std::cout << "Taskflow executor initialized with " 
              << std::thread::hardware_concurrency() 
              << " workers" << std::endl;
}
```

### 6.3. Error Handling for Missing Dependencies

```cpp
// Add to main.cpp
#ifndef __has_include
  #error "Compiler does not support __has_include"
#endif

#if !__has_include(<taskflow/taskflow.hpp>)
  #error "Taskflow not found. Please clone https://github.com/taskflow/taskflow.git to external/taskflow"
#endif

// Check C++ version
#if __cplusplus < 201703L
  #error "This project requires C++17 or later"
#endif
```

## 7. Validation Setup

### 7.1. Create Validation Script

Create `scripts/validate_taskflow.sh`:

```bash
#!/bin/bash
set -e

# Check if reference implementation exists
if [ ! -f "./sta_ref" ]; then
    echo "Error: Reference implementation (sta_ref) not found"
    echo "Please build the reference implementation first"
    exit 1
fi

# Check if Taskflow implementation exists
if [ ! -f "./sta" ]; then
    echo "Error: Taskflow implementation (sta) not found"
    echo "Please build the project first"
    exit 1
fi

# Make script executable
chmod +x scripts/validate_taskflow.sh
```

### 7.2. Environment Setup for Profiling

Create `.env` file for development:

```bash
# Enable Taskflow profiler
export TF_ENABLE_PROFILER=1

# Set thread count (optional, defaults to hardware concurrency)
# export TF_NUM_THREADS=8

# Enable debug output (if implemented)
# export STA_DEBUG=1
```

## 8. Common Issues and Solutions

### 8.1. Compilation Errors

**Issue**: "taskflow/taskflow.hpp not found"
```bash
# Solution: Ensure Taskflow is cloned
cd external
git clone https://github.com/taskflow/taskflow.git
```

**Issue**: "C++17 features not supported"
```bash
# Solution: Update compiler or specify version
cmake -DCMAKE_CXX_COMPILER=g++-11 ..
```

### 8.2. Linking Errors

**Issue**: "undefined reference to pthread_create"
```bash
# Solution: Ensure -pthread flag is set
# Already handled in CMakeLists.txt, but can add manually:
g++ -std=c++17 -pthread your_file.cpp -I./external/taskflow
```

### 8.3. Runtime Errors

**Issue**: "Segmentation fault"
```bash
# Solution: Build with debug symbols and use debugger
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
gdb ./sta
(gdb) run test/NLDM_lib_max2Inp test/cleaned_iscas89_99_circuits/c17.isc
```

## 9. Quick Start Commands for Cursor

Copy and paste these commands to get started quickly:

```bash
# 1. Initial setup
mkdir -p sta_taskflow/{include,src,test,scripts,external,build}
cd sta_taskflow

# 2. Clone Taskflow
git clone https://github.com/taskflow/taskflow.git external/taskflow

# 3. Copy test files from reference (if available)
cp -r ../STA/ref/test/* test/

# 4. Create CMakeLists.txt (paste content from section 4)
nano CMakeLists.txt

# 5. Build
cd build
cmake ..
make -j$(nproc)

# 6. Test
./sta ../test/NLDM_lib_max2Inp ../test/cleaned_iscas89_99_circuits/c17.isc
```

## 10. Verification Checklist

Before starting implementation, verify:

- [ ] Taskflow is cloned in `external/taskflow`
- [ ] CMakeLists.txt is created with correct paths
- [ ] Test files are available in `test/` directory
- [ ] Build directory is created
- [ ] CMake runs without errors
- [ ] Compiler supports C++17 or later
- [ ] pthread library is available

## 11. Additional Resources

- **Taskflow Documentation**: https://taskflow.github.io/taskflow/index.html
- **Taskflow Examples**: `external/taskflow/examples/`
- **Taskflow Cookbook**: https://taskflow.github.io/taskflow/pages.html
- **Profiler (TFProf)**: https://taskflow.github.io/tfprof/

This setup guide ensures all dependencies are properly configured for Cursor to implement the Taskflow-based STA tool successfully.