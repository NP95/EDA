# Compiler and flags
CC = g++
CFLAGS = -Wall -std=c++17 -O2
DEBUG_FLAGS = -DDEBUG -g
INCLUDE = -I../include

# Input files
SRC = PA1Solution.cpp
TEMP_SRC = PA1Solution_temp.cpp
DEBUG_SRC = ../src/debug.cpp
DEBUG_TEMP_SRC = PA1Solution_debug_temp.cpp
INPUT_LIB = NLDM_lib_max2Inp
INPUT_FILE = c17.isc

# Executable names
TARGET = sta
DEBUG_TARGET = sta_debug

# Default target
all: $(TARGET)

# Debug target
debug: $(DEBUG_TARGET)

# Create stub debug header file
debug_stub.hpp:
	@echo "// Creating debug_stub.hpp with empty implementations"
	@echo "// debug_stub.hpp - Empty debug implementation for standard build" > debug_stub.hpp
	@echo "#ifndef DEBUG_STUB_HPP" >> debug_stub.hpp
	@echo "#define DEBUG_STUB_HPP" >> debug_stub.hpp
	@echo "" >> debug_stub.hpp
	@echo "#include <string>" >> debug_stub.hpp
	@echo "#include <cmath>  // For fabs, isinf" >> debug_stub.hpp
	@echo "#include <limits> // For infinity" >> debug_stub.hpp
	@echo "#include <vector>" >> debug_stub.hpp
	@echo "" >> debug_stub.hpp
	@echo "// Empty Debug namespace with stub functions that do nothing" >> debug_stub.hpp
	@echo "namespace Debug {" >> debug_stub.hpp
	@echo "    // Debug levels (unused but needed for compilation)" >> debug_stub.hpp
	@echo "    enum Level {" >> debug_stub.hpp
	@echo "        NONE = 0," >> debug_stub.hpp
	@echo "        ERROR = 1," >> debug_stub.hpp
	@echo "        WARN = 2," >> debug_stub.hpp
	@echo "        INFO = 3," >> debug_stub.hpp
	@echo "        DETAIL = 4," >> debug_stub.hpp
	@echo "        TRACE = 5" >> debug_stub.hpp
	@echo "    };" >> debug_stub.hpp
	@echo "    " >> debug_stub.hpp
	@echo "    // Stub initialization and cleanup" >> debug_stub.hpp
	@echo "    inline bool initialize(Level level, const std::string& = \"\") { return true; }" >> debug_stub.hpp
	@echo "    inline void cleanup() {}" >> debug_stub.hpp
	@echo "    " >> debug_stub.hpp
	@echo "    // Stub for setting circuit name" >> debug_stub.hpp
	@echo "    inline void setCircuitName(const std::string&) {}" >> debug_stub.hpp
	@echo "    " >> debug_stub.hpp
	@echo "    // Stubs for all log types" >> debug_stub.hpp
	@echo "    inline void error(const std::string&) {}" >> debug_stub.hpp
	@echo "    inline void warn(const std::string&) {}" >> debug_stub.hpp
	@echo "    inline void info(const std::string&) {}" >> debug_stub.hpp
	@echo "    inline void detail(const std::string&) {}" >> debug_stub.hpp
	@echo "    inline void trace(const std::string&) {}" >> debug_stub.hpp
	@echo "    inline void log(Level, const std::string&) {}" >> debug_stub.hpp
	@echo "    " >> debug_stub.hpp
	@echo "    // Stubs for specialized trace functions" >> debug_stub.hpp
	@echo "    template<typename... Args>" >> debug_stub.hpp
	@echo "    inline void traceInterpolation(Args&&...) {}" >> debug_stub.hpp
	@echo "    " >> debug_stub.hpp
	@echo "    template<typename... Args>" >> debug_stub.hpp
	@echo "    inline void traceGateDelay(Args&&...) {}" >> debug_stub.hpp
	@echo "    " >> debug_stub.hpp
	@echo "    template<typename... Args>" >> debug_stub.hpp
	@echo "    inline void dumpCircuitState(Args&&...) {}" >> debug_stub.hpp
	@echo "    " >> debug_stub.hpp
	@echo "    template<typename... Args>" >> debug_stub.hpp
	@echo "    inline void dumpLibraryTables(Args&&...) {}" >> debug_stub.hpp
	@echo "}" >> debug_stub.hpp
	@echo "" >> debug_stub.hpp
	@echo "#endif // DEBUG_STUB_HPP" >> debug_stub.hpp

# Create temporary source file for standard build
$(TEMP_SRC): $(SRC) debug_stub.hpp
	@echo "// Creating temporary source with debug stub"
	@sed 's/#include "debug.hpp"/#include "debug_stub.hpp"/' $(SRC) > $(TEMP_SRC)

# Create temporary source file for debug build with extra headers
$(DEBUG_TEMP_SRC): $(SRC)
	@echo "// Creating temporary source for debug build with extra headers"
	@echo "#include <iostream>" > $(DEBUG_TEMP_SRC)
	@echo "#include <fstream>" >> $(DEBUG_TEMP_SRC)
	@echo "#include <sstream>" >> $(DEBUG_TEMP_SRC)
	@echo "#include <vector>" >> $(DEBUG_TEMP_SRC)
	@echo "#include <algorithm>" >> $(DEBUG_TEMP_SRC)
	@echo "#include <string>" >> $(DEBUG_TEMP_SRC)
	@echo "#include <stdlib.h>" >> $(DEBUG_TEMP_SRC)
	@echo "#include <unordered_map>" >> $(DEBUG_TEMP_SRC)
	@echo "#include <set>" >> $(DEBUG_TEMP_SRC)
	@echo "#include <queue>" >> $(DEBUG_TEMP_SRC)
	@echo "#include <climits>" >> $(DEBUG_TEMP_SRC)
	@echo "#include <iomanip>" >> $(DEBUG_TEMP_SRC)
	@echo "#include <cmath>" >> $(DEBUG_TEMP_SRC)
	@echo "#include <limits>" >> $(DEBUG_TEMP_SRC)
	@echo "#include \"debug.hpp\"" >> $(DEBUG_TEMP_SRC)
	@echo "#include <cstdlib>" >> $(DEBUG_TEMP_SRC)
	@cat $(SRC) | grep -v "#include" >> $(DEBUG_TEMP_SRC)

# Run target - executes the program with required parameters
run: $(TARGET)
	./$(TARGET) $(INPUT_LIB) $(INPUT_FILE)

# Run debug target with different log levels
run_trace: $(DEBUG_TARGET)
	STA_DEBUG_LEVEL=TRACE ./$(DEBUG_TARGET) $(INPUT_LIB) $(INPUT_FILE)

run_detail: $(DEBUG_TARGET)
	STA_DEBUG_LEVEL=DETAIL ./$(DEBUG_TARGET) $(INPUT_LIB) $(INPUT_FILE)

run_info: $(DEBUG_TARGET)
	STA_DEBUG_LEVEL=INFO ./$(DEBUG_TARGET) $(INPUT_LIB) $(INPUT_FILE)

# Normal compilation rule - using modified source with stub
$(TARGET): debug_stub.hpp $(TEMP_SRC)
	$(CC) $(CFLAGS) -o $@ $(TEMP_SRC)

# Debug compilation rule with patched source
$(DEBUG_TARGET): $(DEBUG_TEMP_SRC) $(DEBUG_SRC)
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) $(INCLUDE) -o $@ $^

# Removes executable and any temporary files
clean:
	rm -f $(TARGET) $(DEBUG_TARGET) *.o *~ *.log debug_stub.hpp $(TEMP_SRC) $(DEBUG_TEMP_SRC) original_output.txt debug_output.txt

# Compare outputs between original and debug versions
compare: $(TARGET) $(DEBUG_TARGET)
	@echo "=== Running original version ==="
	./$(TARGET) $(INPUT_LIB) $(INPUT_FILE)
	@mv ckt_traversal.txt original_output.txt
	@echo "=== Running debug version ==="
	STA_DEBUG_LEVEL=INFO ./$(DEBUG_TARGET) $(INPUT_LIB) $(INPUT_FILE)
	@mv ckt_traversal.txt debug_output.txt
	@echo "=== Comparing outputs ==="
	@diff -q original_output.txt debug_output.txt || echo "Outputs differ! Use 'diff original_output.txt debug_output.txt' to see differences."

# Phony targets declaration
.PHONY: all debug run run_trace run_detail run_info clean compare