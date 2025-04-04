#include "Netlist.h"
#include <stdexcept>

namespace fm {

void Netlist::addCell(const std::string& name) {
    // Check if cell already exists
    if (cellNameToId_.find(name) != cellNameToId_.end()) {
        return;  // Cell already exists
    }

    // Create new cell
    Cell cell;
    cell.name = name;
    cell.id = static_cast<int>(cells_.size());
    
    // Add to containers
    cells_.push_back(cell);
    cellNameToId_[name] = cell.id;
}

Cell* Netlist::getCellByName(const std::string& name) {
    auto it = cellNameToId_.find(name);
    if (it == cellNameToId_.end()) {
        return nullptr;
    }
    return &cells_[it->second];
}

Cell* Netlist::getCellById(int id) {
    if (id < 0 || id >= static_cast<int>(cells_.size())) {
        return nullptr;
    }
    return &cells_[id];
}

void Netlist::addNet(const std::string& name) {
    // Check if net already exists
    if (netNameToId_.find(name) != netNameToId_.end()) {
        return;  // Net already exists
    }

    // Create new net
    Net net;
    net.name = name;
    net.id = static_cast<int>(nets_.size());
    
    // Add to containers
    nets_.push_back(net);
    netNameToId_[name] = net.id;
}

void Netlist::addCellToNet(const std::string& netName, const std::string& cellName) {
    // Get net and cell
    Net* net = getNetByName(netName);
    Cell* cell = getCellByName(cellName);
    
    if (!net || !cell) {
        throw std::runtime_error("Net or cell not found when adding cell to net");
    }

    // Add bidirectional connections
    net->cells.push_back(cell);
    cell->nets.push_back(net);

    // Update partition count for the net
    net->partitionCount[cell->partition]++;
}

Net* Netlist::getNetByName(const std::string& name) {
    auto it = netNameToId_.find(name);
    if (it == netNameToId_.end()) {
        return nullptr;
    }
    return &nets_[it->second];
}

Net* Netlist::getNetById(int id) {
    if (id < 0 || id >= static_cast<int>(nets_.size())) {
        return nullptr;
    }
    return &nets_[id];
}

} // namespace fm 