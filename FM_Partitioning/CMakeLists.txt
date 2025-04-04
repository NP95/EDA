cmake_minimum_required(VERSION 3.10)
project(FM_Partitioner)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3")
endif()

# Add source files
set(SOURCES
    main.cpp
    DataStructures/Netlist.cpp
    DataStructures/PartitionState.cpp
    DataStructures/GainBucket.cpp
    IO/Parser.cpp
    IO/OutputGenerator.cpp
    Algorithm/FMEngine.cpp
)

# Create executable
add_executable(fm ${SOURCES})

# Add include directories
target_include_directories(fm PRIVATE ${CMAKE_SOURCE_DIR}) 