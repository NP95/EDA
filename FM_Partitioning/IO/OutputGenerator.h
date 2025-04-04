#pragma once

#include <string>
#include "../DataStructures/Netlist.h"
#include "../DataStructures/PartitionState.h"

namespace fm {

class OutputGenerator {
public:
    // Constructor
    OutputGenerator() = default;

    // Generate output file
    bool generateOutput(const std::string& filename, 
                       const Netlist& netlist,
                       const PartitionState& state);

private:
    // Helper methods
    void writeCutSize(std::ostream& out, int cutSize);
    void writePartition(std::ostream& out, const Netlist& netlist, 
                       int partitionId, const std::string& label);
};

} // namespace fm 