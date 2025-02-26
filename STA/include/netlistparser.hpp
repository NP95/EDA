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
    
    // New scanner-based parsing methods
    bool parseScannerInputs();
    bool parseScannerOutputs();
    bool parseScannerDFF();
    bool parseScannerGate();

public:
    NetlistParser(const std::string& filename, Circuit& circuit, bool useScanner = true)
        : Parser(filename, useScanner), circuit_(circuit) {}
    
    bool parse() override;
};

#endif // NETLIST_PARSER_HPP