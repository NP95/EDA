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