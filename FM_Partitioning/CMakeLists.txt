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

# --- Add Custom Run Targets ---

# Find input files
set(INPUT_DIR ${CMAKE_SOURCE_DIR}/input_pa1)
file(GLOB INPUT_FILES RELATIVE ${INPUT_DIR} ${INPUT_DIR}/input_*.dat)

message(STATUS "Found input files: ${INPUT_FILES}") # DEBUG MESSAGE

set(RUN_TARGETS)

foreach(INPUT_FILE ${INPUT_FILES})
    # Extract the number from the filename (e.g., input_0.dat -> 0)
    string(REGEX MATCH "input_([0-9]+)\.dat" MATCH_RESULT ${INPUT_FILE})
    
    if(CMAKE_MATCH_COUNT GREATER 0) # Check if the regex matched at all
        set(RUN_INDEX ${CMAKE_MATCH_1}) # Use the captured group 1
        set(TARGET_NAME run${RUN_INDEX})
        set(OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/output_pa1_${RUN_INDEX}.txt)
        set(INPUT_FILE_FULL_PATH ${INPUT_DIR}/${INPUT_FILE})

        message(STATUS "Creating target: ${TARGET_NAME} for input ${INPUT_FILE}") # DEBUG MESSAGE

        # Add individual run target
        add_custom_target(${TARGET_NAME}
            COMMAND ${CMAKE_COMMAND} -E echo "Running ${TARGET_NAME}: ${INPUT_FILE}"
            COMMAND $<TARGET_FILE:fm> ${INPUT_FILE_FULL_PATH} ${OUTPUT_FILE}
            COMMAND ${CMAKE_SOURCE_DIR}/checker_linux ${INPUT_FILE_FULL_PATH} ${OUTPUT_FILE}
            DEPENDS fm
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR} # Run from build directory
            COMMENT "Running FM and Checker on ${INPUT_FILE}"
            VERBATIM
            USES_TERMINAL
        )
        list(APPEND RUN_TARGETS ${TARGET_NAME})
    else()
        message(WARNING "Could not extract run index from: ${INPUT_FILE}") # DEBUG MESSAGE
    endif()
endforeach()

message(STATUS "Created run targets: ${RUN_TARGETS}") # DEBUG MESSAGE

# Add target to run all tests
if(RUN_TARGETS)
    add_custom_target(run-all
        COMMENT "Running all FM test cases"
    )
    add_dependencies(run-all ${RUN_TARGETS})
endif() 