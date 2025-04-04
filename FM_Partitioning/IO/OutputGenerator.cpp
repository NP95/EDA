#include "OutputGenerator.h"
#include <fstream>
#include <algorithm>

namespace fm {

bool OutputGenerator::generateOutput(const std::string& filename,
                                  const Netlist& netlist,
                                  const PartitionState& state) {
    std::ofstream file(filename);
    if (!file) {
        return false;
    }

    // Write cut size
    writeCutSize(file, state.getCurrentCutSize());

    // Write G1 partition
    writePartition(file, netlist, 0, "G1");

    // Write G2 partition
    writePartition(file, netlist, 1, "G2");

    return true;
}

void OutputGenerator::writeCutSize(std::ostream& out, int cutSize) {
    out << "Cutsize = " << cutSize << "\n";
}

void OutputGenerator::writePartition(std::ostream& out, const Netlist& netlist,
                                   int partitionId, const std::string& label) {
    // Collect cells in this partition
    std::vector<std::string> cellNames;
    for (const auto& cell : netlist.getCells()) {
        if (cell.partition == partitionId) {
            cellNames.push_back(cell.name);
        }
    }

    // Sort cell names for consistent output
    std::sort(cellNames.begin(), cellNames.end());

    // Write partition header with size
    out << label << " " << cellNames.size();

    // Write cell names
    for (const auto& name : cellNames) {
        out << " " << name;
    }
    out << " ;\n";
}

} // namespace fm 