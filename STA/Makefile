# Makefile for STA Project

# Compiler and Flags
CXX = g++

# Release flags (Optimized)
CXXFLAGS_RELEASE = -std=c++17 -Wall -Wextra -g -O2

# Debug flags (No optimization, Debug symbols)
CXXFLAGS_DEBUG = -std=c++17 -Wall -Wextra -g -O0 -DDEBUG_BUILD

# Linker flags (if any)
LDFLAGS = -lm -lfmt # Link math library and fmt library
INCLUDE_DIRS = -Iinclude

# Directories
SRCDIR = src
OBJDIR = obj

# Executables
STA_TARGET = sta
VALIDATE_TARGET = validate_interpolation

# Object files needed for the main STA target
STA_OBJECTS = $(OBJDIR)/Circuit.o $(OBJDIR)/Debug.o $(OBJDIR)/Gate.o $(OBJDIR)/GateLibrary.o $(OBJDIR)/Node.o $(OBJDIR)/main.o

# Object files needed for the validation program
VALIDATE_OBJECTS = $(OBJDIR)/Debug.o $(OBJDIR)/Gate.o $(OBJDIR)/GateLibrary.o $(OBJDIR)/validate_interpolation.o

# Targets
.PHONY: all clean debug release validate

# Default target is release build of STA
all: release

# Build STA in release mode
release: CXXFLAGS = $(CXXFLAGS_RELEASE)
release: $(STA_TARGET)

# Build STA in debug mode
debug: CXXFLAGS = $(CXXFLAGS_DEBUG)
debug: $(STA_TARGET)

# Build validation program
validate: CXXFLAGS = $(CXXFLAGS_RELEASE)
validate: $(VALIDATE_TARGET)

# Link the main STA target
$(STA_TARGET): $(STA_OBJECTS)
	@echo "Linking $(STA_TARGET)..."
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "Build complete: ./$@"

# Link the validation program
$(VALIDATE_TARGET): $(VALIDATE_OBJECTS)
	@echo "Linking $(VALIDATE_TARGET)..."
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "Build complete: ./$@"

# Pattern rule for object files
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	@echo "Compiling $< (Pattern Rule)..."
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) -c $< -o $@

# Create object directory if it doesn't exist
$(OBJDIR):
	@mkdir -p $(OBJDIR)

# Clean up generated files
clean:
	@echo "Cleaning up generated files..."
	rm -rf $(OBJDIR) ./$(STA_TARGET) ./$(VALIDATE_TARGET)
	@echo "Cleanup complete." 