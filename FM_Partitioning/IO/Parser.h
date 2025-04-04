#pragma once

#include <string>
#include "../DataStructures/Netlist.h"

namespace fm {

class Parser {
public:
    // Constructor
    Parser() = default;

    // Parse input file and populate netlist
    bool parseInput(const std::string& filename, double& balanceFactor, Netlist& netlist);

private:
    // Helper methods
    bool parseBalanceFactor(const std::string& line, double& balanceFactor);
    bool parseNetLine(const std::string& line, Netlist& netlist);
};

} // namespace fm 