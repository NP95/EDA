// parser.hpp
#ifndef PARSER_HPP
#define PARSER_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <memory>

// Forward declarations
class Circuit;
class Library;

// Base Parser class
class Parser {
protected:
    std::string filename_;
    std::ifstream file_;
    
    // Helper methods
    bool openFile();
    std::string getLine();
    std::vector<std::string> tokenize(const std::string& line, char delimiter = ' ');

public:
    explicit Parser(const std::string& filename) : filename_(filename) {}
    virtual ~Parser() { if (file_.is_open()) file_.close(); }
    
    // Pure virtual function
    virtual bool parse() = 0;
};

// NetlistParser declaration
class NetlistParser : public Parser {
private:
    Circuit& circuit_;
    
    // Private parsing functions
    bool parseInputs(const std::string& line);
    bool parseOutputs(const std::string& line);
    bool parseGate(const std::string& line);
    bool parseDFF(const std::string& line);

public:
    NetlistParser(const std::string& filename, Circuit& circuit) 
        : Parser(filename), circuit_(circuit) {}
    
    bool parse() override;
};

// LibertyParser declaration
class LibertyParser : public Parser {
private:
    Library& library_;
    
    // Private parsing functions
    bool parseDelayTable(const std::string& gate_type);
    bool parseSlewTable(const std::string& gate_type);
    bool parseCapacitance(const std::string& line);

public:
    LibertyParser(const std::string& filename, Library& library) 
        : Parser(filename), library_(library) {}
    
    bool parse() override;
};

#endif // PARSER_HPP