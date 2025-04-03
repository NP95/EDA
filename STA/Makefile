# Makefile for the STA Tool (Handles src/ and include/ directories)

# Compiler
CXX = g++

# Add DEBUG_MODE variable, default is 0 (Release)
DEBUG_MODE ?= 0

# Base flags
CXXFLAGS_BASE = -std=c++17 -Wall -Wextra -g -I$(INCDIR) # Using C++17 as seen in your output, added -g for symbols

# Conditional flags
ifeq ($(DEBUG_MODE), 1)
  # Debug build: Enable DEBUG_BUILD macro, maybe less optimization
  CXXFLAGS = $(CXXFLAGS_BASE) -O0 -DDEBUG_BUILD
else
  # Release build: No DEBUG_BUILD macro, enable optimization
  CXXFLAGS = $(CXXFLAGS_BASE) -O2
endif

# Linker flags (if any needed, e.g., -lm for math library)
LDFLAGS =

# Directories
SRCDIR = src
INCDIR = include
OBJDIR = obj

# Name of the final executable
TARGET = sta_tool

# Find all .cpp files in the source directory
SRCS = $(wildcard $(SRCDIR)/*.cpp)

# Generate object file names in the object directory
# e.g., src/Utils.cpp -> obj/Utils.o
OBJS = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS))

# --- Rules ---

# Default target: Build the executable
all: $(TARGET)

# Rule to link the executable from object files
# Depends on all object files in $(OBJDIR)
$(TARGET): $(OBJS)
	@echo "Linking $(TARGET)..."
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "$(TARGET) built successfully."

# Rule to compile a .cpp source file from SRCDIR into a .o object file in OBJDIR
# Depends on the corresponding .cpp file in SRCDIR
# Also good practice to make it depend on the OBJDIR existing
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Rule to create the object directory if it doesn't exist
# This is used as an order-only prerequisite (via |) for the compilation rule
$(OBJDIR):
	@mkdir -p $(OBJDIR)

# Rule to clean up generated files
clean:
	@echo "Cleaning up generated files..."
	rm -rf $(OBJDIR) $(TARGET) # Use rm -rf to remove obj directory safely
	@echo "Cleanup complete."

# Declare targets that are not actual files
.PHONY: all clean $(OBJDIR)