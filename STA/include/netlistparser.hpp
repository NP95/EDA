// netlistparser.hpp
#ifndef NETLIST_PARSER_HPP
#define NETLIST_PARSER_HPP

#include "parser.hpp"
#include "circuit.hpp"

class NetlistParser : public Parser {
private:
    Circuit& circuit_;
    
    // Parsing methods for different circuit elements
    // All methods consistently take a line parameter
    bool parseInputs(const std::string& line);
    bool parseOutputs(const std::string& line);
    bool parseDFF(const std::string& line);
    bool parseGate(const std::string& line);
    
    // Scanner-based parsing methods
    bool parseScannerInputs(const std::string& line, int lineNum); // Add lineNum
    bool parseScannerOutputs(const std::string& line, int lineNum); // Add lineNum
    bool parseScannerDFF(const std::string& line, int lineNum);    // Add lineNum
    bool parseScannerGate(const std::string& line, int lineNum);   // Add lineNum

public:
    NetlistParser(const std::string& filename, Circuit& circuit, bool useScanner = true)
        : Parser(filename, useScanner), circuit_(circuit) {}
    
    bool parse() override;
};

#endif // NETLIST_PARSER_HPP