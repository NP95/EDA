# Compiler and flags
CC = g++
CFLAGS = -Wall -std=c++17

# Input files
SRC = PA1Solution.cpp
INPUT_LIB = NLDM_lib_max2Inp
INPUT_FILE = c17.isc

# Executable name
TARGET = sta

# Default target
all: $(TARGET)

# Run target - executes the program with required parameters
run: $(TARGET)
	./$(TARGET) $(INPUT_LIB) $(INPUT_FILE)

# Compilation rule
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $< -o $@

# Removes executable and any temporary files
clean:
	rm -f $(TARGET) *.o *~

# Test target - same as run in this case, but separated for clarity
test: $(TARGET)
	./$(TARGET) $(INPUT_LIB) $(INPUT_FILE)

# Phony targets declaration
.PHONY: all run clean test