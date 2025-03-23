# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -O2 -pg -I$(INCDIR)

# Directories
SRCDIR = src
INCDIR = include
OBJDIR = obj
BINDIR = bin

# Target executable
TARGET = $(BINDIR)/sta

# Source files
SRCS := $(wildcard $(SRCDIR)/*.cpp)
OBJS := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS))

# Default target
all: directories $(TARGET)

# Create necessary directories
directories:
	mkdir -p $(OBJDIR)
	mkdir -p $(BINDIR)

# Link the executable
$(TARGET): $(OBJS)
	$(CXX) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Linking complete!"

# Compile source files
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@
	@echo "Compiled $<"

# Clean up
clean:
	rm -rf $(OBJDIR) $(BINDIR)
	@echo "Cleanup complete!"

# Run the program with two arguments
run: all
	./$(TARGET) $(ARGS)

# Phony targets
.PHONY: all clean directories run