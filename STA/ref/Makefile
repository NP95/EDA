# Compiler and flags
CC = g++
CFLAGS = -Wall -std=c++17

# Executable name
TARGET = sta

run: $(TARGET)
	./$(TARGET) NLDM_lib_max2Inp c17.isc

$(TARGET): PA1Solution_kumaw010.cpp
	$(CC) $(CFLAGS) PA1Solution_kumaw010.cpp -o $(TARGET)

clean:
	rm -f $(TARGET)

test: $(TARGET)
	./$(TARGET) NLDM_lib_max2Inp c17.isc

.PHONY: all run clean test
