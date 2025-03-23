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
    void processValuesLine(const std::string& line, std::vector<std::vector<double>>& tableValues, int& rowCount);


    
};

#endif // LIBERTY_PARSER_HPP