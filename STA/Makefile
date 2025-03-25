# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -O2 -I$(INCDIR)
DEBUGFLAGS = -g -DDEBUG

# Directories
SRCDIR = src
INCDIR = include
OBJDIR = obj
BINDIR = bin
VALDIR = validation

# Target executable
TARGET = $(BINDIR)/sta

# Source files and objects
SRCS := $(wildcard $(SRCDIR)/*.cpp)
OBJS := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS))

# Validation script
VALIDATION_SCRIPT = ./validation.sh

# Default target
all: directories $(TARGET)

# Create necessary directories
directories:
	mkdir -p $(OBJDIR)
	mkdir -p $(BINDIR)
	mkdir -p $(VALDIR)

# Link the executable
$(TARGET): $(OBJS)
	$(CXX) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Linking complete!"

# Compile source files
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@
	@echo "Compiled $<"

# Debug build with additional flags
debug: CXXFLAGS += $(DEBUGFLAGS)
debug: all

# Clean build artifacts
clean:
	rm -rf $(OBJDIR) $(BINDIR)
	@echo "Cleanup complete!"

# Clean validation artifacts
clean-validation:
	rm -rf $(VALDIR)/* cleaned_circuits/*
	@echo "Validation cleanup complete!"

# Clean everything
clean-all: clean clean-validation

# Run the program with arguments
run: all
	./$(TARGET) $(ARGS)

# Validation targets that invoke the script with different options
validate: all
	$(VALIDATION_SCRIPT)

validate-debug: all
	$(VALIDATION_SCRIPT) -d

# Validate a specific circuit with debug tracing
validate-circuit: all
	@if [ -z "$(CIRCUIT)" ]; then \
		echo "Error: CIRCUIT variable not set. Use 'make validate-circuit CIRCUIT=c17.isc'"; \
		exit 1; \
	fi
	$(VALIDATION_SCRIPT) -d -s $(CIRCUIT)

# Just clean the circuit files
clean-circuits:
	$(VALIDATION_SCRIPT) -c

# Just build both implementations
build-all:
	$(VALIDATION_SCRIPT) -b

# Generate a detailed debug report for a specific circuit
debug-report: all
	@if [ -z "$(CIRCUIT)" ]; then \
		echo "Error: CIRCUIT variable not set. Use 'make debug-report CIRCUIT=c17.isc'"; \
		exit 1; \
	fi
	$(VALIDATION_SCRIPT) -d -s $(CIRCUIT)
	@echo "Debug report generated in $(VALDIR)/debug_report_$(CIRCUIT).log"
	@grep -A 3 "Circuit delay:" ckt_traversal.txt >> $(VALDIR)/debug_report_$(CIRCUIT).log
	@cat sta_debug.log >> $(VALDIR)/debug_report_$(CIRCUIT).log

# Help target
help:
	@echo "Available targets:"
	@echo "  all              : Build the STA tool (default target)"
	@echo "  debug            : Build with debug symbols and DEBUG flag"
	@echo "  clean            : Remove build artifacts"
	@echo "  clean-validation : Remove validation artifacts"
	@echo "  clean-all        : Remove all artifacts"
	@echo "  run ARGS=...     : Run STA with specified arguments"
	@echo "  validate         : Run full validation on all circuits"
	@echo "  validate-debug   : Run validation with debug tracing"
	@echo "  validate-circuit CIRCUIT=... : Validate a specific circuit with debug"
	@echo "  clean-circuits   : Just clean the circuit files"
	@echo "  build-all        : Build both implementations"
	@echo "  debug-report CIRCUIT=... : Generate detailed debug report"
	@echo "  help             : Show this help message"

# Phony targets
.PHONY: all debug clean clean-validation clean-all run validate validate-debug \
        validate-circuit clean-circuits build-all debug-report help directories