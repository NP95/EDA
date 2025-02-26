# EE5301: Advanced Digital VLSI Design
## Programming Assignment: Building a Static Timing Analysis (STA) Tool

### Overview

In this assignment, you will implement a Static Timing Analysis (STA) tool that analyzes digital circuits to compute signal propagation delays and identify critical paths. Your tool will read netlists in a standard format, incorporate timing information from a Liberty file, and perform precise delay calculations.

The assignment is divided into two phases:
1. **Circuit Parsing Phase**: Implementing an efficient parser for netlists and timing models
2. **Timing Analysis Phase**: Performing actual static timing analysis (to be completed in a later assignment)

This assignment focuses exclusively on the Circuit Parsing Phase, creating the foundation for the timing analysis to follow.

### Learning Objectives

Upon completing this assignment, you will be able to:
- Implement high-performance parsing algorithms for large circuit descriptions
- Design efficient data structures for representing digital circuits
- Process industry-standard design files (netlist and liberty formats)
- Apply performance optimization techniques for handling large datasets
- Develop a foundation for implementing static timing analysis algorithms

### Background: Static Timing Analysis

Static Timing Analysis (STA) is an essential step in digital circuit design that verifies whether a circuit meets its timing requirements. Unlike dynamic simulation, STA analyzes all possible paths through the circuit without requiring test vectors, making it computationally efficient for large designs.

The key concepts in STA include:
- **Gate Delay**: Time taken for a signal to propagate through a logic gate
- **Net Delay**: Time taken for a signal to travel along interconnections
- **Arrival Time**: Time when a signal reaches a particular point in the circuit
- **Required Time**: Time by which a signal must reach a point to meet timing requirements
- **Slack**: Difference between required time and arrival time (positive slack means timing is met)
- **Critical Path**: Path with the minimum slack, determining the maximum operating frequency

### Assignment Requirements

You will build a tool that:

1. Parses circuit netlists in the ISCAS 89/99 format
2. Parses timing models in the Liberty format (simplified version provided)
3. Constructs an efficient in-memory graph representation of the circuit
4. Prepares data structures for subsequent timing analysis

Your implementation should handle circuits with up to 150,000 gates efficiently.

### Preparation: Understanding the Input Files

#### ISCAS 89/99 Netlist Format

The netlist file describes the circuit structure with the following format:

```
INPUT ( <pin_name> )
...
OUTPUT ( <pin_name> )
...
<gate_id> = <gate_type> ( <input_list> )
...
<dff_id> = DFF ( <input_pin> )
...
```

An example snippet:
```
INPUT ( 1 )
INPUT ( 2 )
OUTPUT ( 25 )
52 = DFF ( 53 )
219 = NAND ( 52, 54 )
```

#### Liberty File Format

The Liberty file provides timing information for different gate types:

```
cell(<cell_name>) {
  capacitance : <value>;
  
  index_1("<input_slew_values>");
  index_2("<load_capacitance_values>");
  
  cell_rise() {
    values("<delay_values>");
  }
  
  rise_transition() {
    values("<output_slew_values>");
  }
}
```

### Getting Started

1. Download the starter code package containing:
   - Directory structure
   - Header files with class definitions
   - Makefile
   - Test circuits of varying sizes
   - Sample Liberty file

2. Familiarize yourself with the provided code and project structure:
   - `include/`: Header files defining the interfaces
   - `src/`: Source files where you'll implement the functionality
   - `cleaned_iscas89_99_circuits/`: Test circuit files
   - `NLDM_lib_max2Inp`: Sample Liberty file

### Assignment Tasks

Your specific tasks are:

1. **Implement the TokenScanner class** for high-performance file parsing
2. **Complete the Parser, NetlistParser, and LibertyParser classes**
3. **Implement Circuit and Library data structures** to represent the circuit graph and timing models
4. **Test your implementation** with provided benchmark circuits

Let's proceed with a more detailed breakdown of each task:

## Task 1: Implement the TokenScanner (30 points)

The TokenScanner class provides high-performance parsing capabilities by reading the entire file into memory at once and scanning through it without creating excessive string objects.

File: `include/tokenscanner.hpp` and `src/tokenscanner.cpp`

Key functions to implement:
- `loadFile()`: Efficiently read an entire file into memory
- `hasMoreTokens()`: Check if there are more tokens to process
- `skipWhitespaceAndComments()`: Skip over whitespace and comment lines
- `nextToken()`: Extract the next token from the buffer
- `getLine()`: Extract a complete line from the buffer
- `peekToken()`: Look at the next token without consuming it
- `consumeIf()`: Conditionally consume a token if it matches expected string

## Task 2: Implement the Parser Classes (30 points)

Implement the base Parser class and its derived classes for specific file formats.

Files:
- `include/parser.hpp` and `src/parser.cpp`
- `include/netlistparser.hpp` and `src/netlistparser.cpp`
- `include/libertyparser.hpp` and `src/libertyparser.cpp`

Key functions to implement:
- `Parser::initialize()`: Set up the parsing environment
- `Parser::getLine()`: Get the next line from the file
- `Parser::tokenize()`: Split a line into tokens
- `NetlistParser::parse()`: Parse a netlist file
- `NetlistParser::parseScannerInputs()`: Parse input declarations
- `NetlistParser::parseScannerOutputs()`: Parse output declarations
- `NetlistParser::parseScannerDFF()`: Parse flip-flop declarations
- `NetlistParser::parseScannerGate()`: Parse gate declarations
- `LibertyParser::parse()`: Parse a liberty file

## Task 3: Implement Circuit and Library Classes (30 points)

Implement the data structures to represent the circuit and timing models.

Files:
- `include/circuit.hpp` and `src/circuit.cpp`
- `include/library.hpp` and `src/library.cpp`

Key functions to implement:
- `Circuit::addNode()`: Add a node to the circuit graph
- `Circuit::addConnection()`: Add a connection between nodes
- `Circuit::getNodeCount()`: Get the total number of nodes
- `Circuit::getPrimaryInputCount()`: Get the number of primary inputs
- `Circuit::getPrimaryOutputCount()`: Get the number of primary outputs
- `Circuit::getNodeTypeCounts()`: Get the distribution of node types
- `Library::DelayTable::interpolate()`: Perform 2D interpolation for timing values
- `Library::getDelay()`: Get the delay for a gate based on input slew and load capacitance
- `Library::getOutputSlew()`: Get the output slew for a gate based on input slew and load capacitance

## Task 4: Main Program Integration (10 points)

Integrate your implementations into a complete program that reads circuit and liberty files and reports statistics.

File: `src/main.cpp`

Key functions:
- `main()`: Parse command-line arguments and orchestrate the parsing process
- `printCircuitStats()`: Print statistics about the parsed circuit
- `printLibraryStats()`: Print statistics about the parsed liberty file

## Starter Code (Skeleton Files)

Here are the skeleton files you'll need to complete for this assignment:

### include/tokenscanner.hpp

```cpp
// tokenscanner.hpp
#ifndef TOKEN_SCANNER_HPP
#define TOKEN_SCANNER_HPP

#include <string>
#include <fstream>
#include <vector>
#include <cctype>
#include <cstring>

class TokenScanner {
private:
    std::vector<char> buffer_;
    const char* current_;
    const char* end_;
    
public:
    // Constructor that loads file directly
    explicit TokenScanner(const std::string& filename) {
        loadFile(filename);
    }
    
    // Constructor that uses an existing buffer
    TokenScanner(const char* buffer, size_t size) 
        : buffer_(buffer, buffer + size), 
          current_(buffer_.data()), 
          end_(buffer_.data() + size) {}
    
    bool loadFile(const std::string& filename);
    bool hasMoreTokens() const;
    void skipWhitespaceAndComments();
    std::string nextToken();
    std::string getLine();
    std::string peekToken();
    bool consumeIf(const std::string& expected);
    int getLineNumber() const;
};

#endif // TOKEN_SCANNER_HPP
```

### include/parser.hpp

```cpp
// parser.hpp
#ifndef PARSER_HPP
#define PARSER_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <functional>
#include <optional>
#include <memory>
#include "tokenscanner.hpp"

// Forward Declarations
class Circuit;
class Library;

// A General Parser Class that can be specialized for different file types
class Parser {
protected:
    std::string filename_;
    std::ifstream file_;
    std::unique_ptr<TokenScanner> scanner_;
    bool useScanner_;
    
    // Helper methods for parsing
    bool openFile();
    std::string getLine();
    std::vector<std::string> tokenize(const std::string& line, char delimiter = ' ');
    
    // New helper to decide whether to use scanner based on file size
    bool shouldUseScanner() const;

public:
    explicit Parser(const std::string& filename, bool useScanner = true) 
        : filename_(filename), useScanner_(useScanner) {}
    
    virtual ~Parser() { if (file_.is_open()) file_.close(); }
    
    // Initialize parser - either opens file stream or creates scanner
    virtual bool initialize();
    
    // Pure Virtual Function that derived classes must implement
    virtual bool parse() = 0;
};

#endif //PARSER_HPP
```

### include/netlistparser.hpp

```cpp
// netlistparser.hpp
#ifndef NETLIST_PARSER_HPP
#define NETLIST_PARSER_HPP

#include "parser.hpp"
#include "circuit.hpp"

class NetlistParser : public Parser {
private:
    Circuit& circuit_;
    
    // Parsing methods for different circuit elements
    bool parseInputs(const std::string& line);
    bool parseOutputs(const std::string& line);
    bool parseDFF(const std::string& line);
    bool parseGate(const std::string& line);
    
    // Scanner-based parsing methods
    bool parseScannerInputs(const std::string& line);
    bool parseScannerOutputs(const std::string& line);
    bool parseScannerDFF(const std::string& line);
    bool parseScannerGate(const std::string& line);

public:
    NetlistParser(const std::string& filename, Circuit& circuit, bool useScanner = true)
        : Parser(filename, useScanner), circuit_(circuit) {}
    
    bool parse() override;
};

#endif // NETLIST_PARSER_HPP
```

### include/libertyparser.hpp

```cpp
// libertyparser.hpp
#ifndef LIBERTY_PARSER_HPP
#define LIBERTY_PARSER_HPP

#include "parser.hpp"
#include "library.hpp"

class LibertyParser : public Parser {
private:
    Library& library_;
    
    // Scanner-based parsing methods
    bool parseCell();
    bool parseInputCapacitance();
    bool parseIndexValues(std::vector<double>& values, bool isInputSlew);
    bool parseTable(bool isDelayTable);

public:
    LibertyParser(const std::string& filename, Library& library, bool useScanner = true)
        : Parser(filename, useScanner), library_(library) {}
    
    bool parse() override;
};

#endif // LIBERTY_PARSER_HPP
```

### include/circuit.hpp

```cpp
// circuit.hpp
#ifndef CIRCUIT_HPP
#define CIRCUIT_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <limits>

// Forward declarations
class NetlistParser;

class Circuit {
public:
    struct Node {
        std::string name;
        std::string type;  // Gate type or "INPUT", "OUTPUT", "DFF"
        int numInputs;
        std::vector<size_t> fanins;
        std::vector<size_t> fanouts;
        double arrivalTime;
        double requiredTime;
        double slack;
        double inputSlew;
        double outputSlew;
        double loadCapacitance;
    };

private:
    std::vector<Node> nodes_;
    std::unordered_map<std::string, size_t> nameToId_;
    std::vector<size_t> primaryInputs_;
    std::vector<size_t> primaryOutputs_;
    std::vector<size_t> topoOrder_;

public:
    Circuit() {}
    
    // Methods to add nodes and connections
    size_t addNode(const std::string& name, const std::string& type, int numInputs = 0);
    void addConnection(const std::string& from, const std::string& to);
    
    // Accessor methods
    const Node& getNode(size_t id) const { return nodes_[id]; }
    size_t getNodeId(const std::string& name) const { return nameToId_.at(name); }
    
    // Statistics methods
    size_t getNodeCount() const { return nodes_.size(); }
    size_t getPrimaryInputCount() const { return primaryInputs_.size(); }
    size_t getPrimaryOutputCount() const { return primaryOutputs_.size(); }
    
    // Get node type distribution
    std::unordered_map<std::string, int> getNodeTypeCounts() const;
    
    // Accessors for iteration
    const std::vector<Node>& getNodes() const { return nodes_; }
    const std::vector<size_t>& getPrimaryInputs() const { return primaryInputs_; }
    const std::vector<size_t>& getPrimaryOutputs() const { return primaryOutputs_; }
    
    // Friend class declaration
    friend class NetlistParser;
};

#endif // CIRCUIT_HPP
```

### include/library.hpp

```cpp
// library.hpp
#ifndef LIBRARY_HPP
#define LIBRARY_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>

class Library {
public:
    struct DelayTable {
        std::vector<double> inputSlews;
        std::vector<double> loadCaps;
        std::vector<std::vector<double>> delayValues;
        std::vector<std::vector<double>> slewValues;
        
        double interpolateDelay(double slew, double load) const;
        double interpolateSlew(double slew, double load) const;
        
    private:
        double interpolate(double slew, double load, 
                          const std::vector<std::vector<double>>& table) const;
    };

private:
    std::unordered_map<std::string, DelayTable> gateTables_;
    double inverterCapacitance_;

public:
    Library() : inverterCapacitance_(0.0) {}
    
    double getDelay(const std::string& gateType, double inputSlew, double loadCap, int numInputs = 2) const;
    double getOutputSlew(const std::string& gateType, double inputSlew, double loadCap, int numInputs = 2) const;
    double getInverterCapacitance() const { return inverterCapacitance_; }
    
    // For debugging
    void printTables() const;

    // Friend class declaration to allow parser direct access
    friend class LibertyParser;
};

#endif // LIBRARY_HPP
```

### src/main.cpp

```cpp
// main.cpp
#include <iostream>
#include <string>
#include <chrono>
#include "circuit.hpp"
#include "library.hpp"
#include "netlistparser.hpp"
#include "libertyparser.hpp"

// Helper function to print circuit statistics
void printCircuitStats(const Circuit& circuit) {
    std::cout << "\n====== Circuit Statistics ======\n";
    std::cout << "Total nodes: " << circuit.getNodeCount() << "\n";
    std::cout << "Primary inputs: " << circuit.getPrimaryInputCount() << "\n";
    std::cout << "Primary outputs: " << circuit.getPrimaryOutputCount() << "\n";
    
    auto gateTypeCounts = circuit.getNodeTypeCounts();
    
    std::cout << "\nGate type distribution:\n";
    for (const auto& [type, count] : gateTypeCounts) {
        std::cout << "  " << type << ": " << count << "\n";
    }
}

// Helper function to print library statistics
void printLibraryStats(const Library& library) {
    std::cout << "\n====== Liberty Statistics ======\n";
    // Uncomment to print detailed tables (warning: can be verbose)
    // library.printTables();
    std::cout << "Inverter capacitance: " << library.getInverterCapacitance() << " fF\n";
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <circuit_file> <liberty_file>" << std::endl;
        return 1;
    }
    
    // Create circuit and library objects
    Circuit circuit;
    Library library;
    
    // Measure parsing time
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Parse liberty file
    std::cout << "Parsing liberty file: " << argv[2] << std::endl;
    LibertyParser libParser(argv[2], library);
    if (!libParser.parse()) {
        std::cerr << "Error parsing liberty file: " << argv[2] << std::endl;
        return 1;
    }
    
    // Parse circuit file
    std::cout << "Parsing circuit file: " << argv[1] << std::endl;
    NetlistParser netlistParser(argv[1], circuit);
    if (!netlistParser.parse()) {
        std::cerr << "Error parsing circuit file: " << argv[1] << std::endl;
        return 1;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    std::cout << "\nParsing completed in " << duration.count() << " ms" << std::endl;
    
    // Print statistics about the parsed data
    printCircuitStats(circuit);
    printLibraryStats(library);
    
    std::cout << "\nParsing successful. Ready for timing analysis implementation." << std::endl;
    
    return 0;
}
```

### Makefile

```makefile
# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -O3
LDFLAGS = 

# Directories
SRCDIR = src
INCDIR = include
OBJDIR = obj
BINDIR = bin

# Target executable
TARGET = $(BINDIR)/sta

# Source files
SOURCES = $(wildcard $(SRCDIR)/*.cpp)
OBJECTS = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SOURCES))

# Header files (for dependency tracking)
HEADERS = $(wildcard $(INCDIR)/*.hpp)

# Default target
all: directories $(TARGET)

# Create necessary directories
directories:
	@mkdir -p $(OBJDIR) $(BINDIR)

# Link the executable
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Linking complete!"

# Compile source files
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -I$(INCDIR) -c -o $@ $
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
```

### Testing Your Implementation

Once you've completed the implementation, you can test your code with the following commands:

```bash
# Build the program
make

# Run with a small test circuit
./bin/sta cleaned_iscas89_99_circuits/c17.isc NLDM_lib_max2Inp

# Run with a medium-sized circuit
./bin/sta cleaned_iscas89_99_circuits/c7552.isc NLDM_lib_max2Inp

# Run with a very large circuit
./bin/sta cleaned_iscas89_99_circuits/b19_1.isc NLDM_lib_max2Inp
```

You should see output similar to:

```
Parsing liberty file: NLDM_lib_max2Inp
Parsing circuit file: ./cleaned_iscas89_99_circuits/b19_1.isc
Parsing completed in 228 ms
====== Circuit Statistics ======
Total nodes: 219394
Primary inputs: 24
Primary outputs: 30
Gate type distribution:
  OR: 1440
  NOR: 729
  NOT: 39426
  NAND: 149865
  AND: 21241
  DFF: 6642
  OUTPUT: 27
  INPUT: 24
====== Liberty Statistics ======
Inverter capacitance: 0 fF
Parsing successful. Ready for timing analysis implementation.
```

### Grading Criteria

Your implementation will be graded on the following criteria:

1. **Correctness (50%)**
   - Parser correctly extracts all circuit elements
   - Circuit graph is correctly constructed
   - Liberty file timing information is properly stored
   - All test cases produce expected results

2. **Performance (25%)**
   - Parsing large circuits (e.g., b19_1.isc) efficiently
   - Memory usage remains reasonable for large circuits
   - Implementation handles the specified 150,000 gate requirement

3. **Code Quality (25%)**
   - Code is well-organized and follows good programming practices
   - Functions and classes have clear responsibilities
   - Error handling is robust
   - Comments explain complex sections of code

### Submission Guidelines

Submit your assignment as a zip file containing:
1. All source files (*.cpp)
2. All header files (*.hpp)
3. The Makefile
4. A README.md file explaining:
   - How to compile and run your program
   - Any implementation decisions or optimizations you made
   - Any challenges you encountered and how you solved them
   - Performance analysis of your implementation

### Tips for Success

1. **Start with small test cases**: Begin with the simplest circuits to get your parser working before moving to larger ones.

2. **Implement incrementally**: First parse primary inputs and outputs, then gates, then flip-flops.

3. **Test frequently**: Check that each component is working correctly before moving to the next.

4. **Focus on data structures**: Choose appropriate data structures for efficient circuit representation.

5. **Consider performance early**: The parsing phase needs to be efficient to handle large circuits.

6. **Debug systematically**: Use print statements or a debugger to trace through the parsing process.

Good luck with your implementation! This assignment will provide valuable experience in building performance-critical tools for VLSI design.