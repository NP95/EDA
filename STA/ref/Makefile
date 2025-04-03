# Compiler and flags
CC = g++
CFLAGS = -Wall -std=c++17 -O2
DEBUG_FLAGS = -DDEBUG -g
INCLUDE = -I../include

# Input files
SRC = PA1Solution.cpp
DEBUG_SRC = PA1Solution_debug.cpp
DEBUG_SUPPORT_SRC = ../src/debug.cpp

# Default library and circuit files
LIB_FILE = NLDM_lib_max2Inp
CKT_FILE = cleaned_c17.isc

# Executable names
TARGET = sta
DEBUG_TARGET = sta_debug

# Default target
all: $(TARGET) $(DEBUG_TARGET)

# Build reference (non-debug) version
reference: $(TARGET)

# Build debug version
debug: $(DEBUG_TARGET)

# Normal compilation rule
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

# Debug compilation rule
$(DEBUG_TARGET): $(DEBUG_SRC) $(DEBUG_SUPPORT_SRC)
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) $(INCLUDE) -o $@ $^

# Run reference version
run: $(TARGET)
	./$(TARGET) $(LIB_FILE) $(CKT_FILE)

# Run debug version with different log levels
run_trace: $(DEBUG_TARGET)
	STA_DEBUG_LEVEL=TRACE ./$(DEBUG_TARGET) $(LIB_FILE) $(CKT_FILE)

run_detail: $(DEBUG_TARGET)
	STA_DEBUG_LEVEL=DETAIL ./$(DEBUG_TARGET) $(LIB_FILE) $(CKT_FILE)

run_info: $(DEBUG_TARGET)
	STA_DEBUG_LEVEL=INFO ./$(DEBUG_TARGET) $(LIB_FILE) $(CKT_FILE)

# Removes executable and any temporary files
clean:
	rm -f $(TARGET) $(DEBUG_TARGET) *.o *~ *.log debug_stub.hpp *.txt

# Compare outputs between original and debug versions
compare: $(TARGET) $(DEBUG_TARGET)
	@echo "=== Running original version ==="
	./$(TARGET) $(LIB_FILE) $(CKT_FILE)
	@mv ckt_traversal.txt reference_output.txt
	@echo "=== Running debug version ==="
	STA_DEBUG_LEVEL=INFO ./$(DEBUG_TARGET) $(LIB_FILE) $(CKT_FILE)
	@mv ckt_traversal.txt debug_output.txt
	@echo "=== Comparing outputs ==="
	@diff -q reference_output.txt debug_output.txt > /dev/null && \
	echo "✅ Outputs match!" || \
	echo "❌ Outputs differ! See differences below:" && \
	diff reference_output.txt debug_output.txt

# Phony targets declaration
.PHONY: all reference debug run run_trace run_detail run_info clean compare