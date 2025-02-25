#ifndef PARSER_HPP
#define PARSER_HPP

//Header Guards

#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <functional>
#include <optional>
#include <memory>

//Forward Declarations
class Circuit;
class Library;

//A General Parser Class that can be specialized for different file types
class Parser
{
    protected:
    std::string filename_;
    std::ifstream file_;
    
    //Helper methods for parsing
    bool openFile();
    std::string getLine();
    std::vector<std::string> tokenize(const std::string& line, char delimiter =' ');

    public:
    explicit Parser(const std::string& filename) : filename_(filename) {}
    virtual ~Parser() { if (file_.is_open()) file_.close();}

    //Pure Virtual Function that derived classes must implement
    virtual bool parse() = 0;
};

class LibertyParser : public Parser {
    private:
    Library& library_;

    //Private Parsing functions for different sections
    bool parseDelayTable(const std::string& gate_type);
    bool parseSlowTable(const std::string& gate_type);
    bool parseDelayTable(const std::string& line);

    bool parse() override;
};


#endif //PARSER_HPP
