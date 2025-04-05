#include "Netlist.h"
#include <stdexcept>
#include <iostream>

namespace fm {

void Netlist::addCell(const std::string& name) {
    std::cout << "Adding cell: " << name << std::endl;
    // Check if cell already exists
    if (cellNameToId_.find(name) != cellNameToId_.end()) {
        std::cout << "  Cell already exists" << std::endl;
        return;  // Cell already exists
    }

    // Create new cell
    Cell cell;
    cell.name = name;
    cell.id = static_cast<int>(cells_.size());
    
    // Add to containers
    cells_.push_back(cell);
    cellNameToId_[name] = cell.id;
    std::cout << "  Cell added with ID: " << cell.id << std::endl;
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

// Const version of getCellById
const Cell* Netlist::getCellById(int id) const {
    if (id < 0 || id >= static_cast<int>(cells_.size())) {
        return nullptr;
    }
    return &cells_[id]; // Return const pointer
}

void Netlist::addNet(const std::string& name) {
    std::cout << "Adding net: " << name << std::endl;
    // Check if net already exists
    if (netNameToId_.find(name) != netNameToId_.end()) {
        std::cout << "  Net already exists" << std::endl;
        return;  // Net already exists
    }

    // Create new net
    Net net;
    net.name = name;
    net.id = static_cast<int>(nets_.size());
    net.partitionCount[0] = net.partitionCount[1] = 0;  // Initialize partition counts
    
    // Add to containers
    nets_.push_back(net);
    netNameToId_[name] = net.id;
    std::cout << "  Net added with ID: " << net.id << std::endl;
}

void Netlist::addCellToNet(const std::string& netName, const std::string& cellName) {
    std::cout << "Adding cell " << cellName << " to net " << netName << std::endl;
    
    // Get net and cell
    Net* net = getNetByName(netName);
    Cell* cell = getCellByName(cellName);
    
    if (!net || !cell) {
        throw std::runtime_error("Net or cell not found when adding cell to net");
    }

    // Check if relationship already exists
    // Using IDs now
    int netId = net->id;
    int cellId = cell->id;

    bool cellHasNetId = false;
    for (int existingNetId : cell->netIds) {
        if (existingNetId == netId) {
            cellHasNetId = true;
            break;
        }
    }

    bool netHasCellId = false;
    for (int existingCellId : net->cellIds) {
        if (existingCellId == cellId) {
            netHasCellId = true;
            break;
        }
    }

    // If either relationship exists, ensure both exist
    if (netHasCellId || cellHasNetId) {
        if (!netHasCellId) {
            net->cellIds.push_back(cellId);   // Store cell ID
            // Partition count updates deferred
        }
        if (!cellHasNetId) {
            cell->netIds.push_back(netId);   // Store net ID
        }
        std::cout << "  Connection already existed or added, ensured consistency" << std::endl;
        std::cout << "  Net now has " << net->cellIds.size() << " cell IDs" << std::endl;
        std::cout << "  Cell now has " << cell->netIds.size() << " net IDs" << std::endl;
        return;
    }

    // Add new bidirectional connections
    net->cellIds.push_back(cellId);   // Store cell ID
    cell->netIds.push_back(netId);   // Store net ID

    // Partition count updates deferred
    
    std::cout << "  Connection added. Net now has " << net->cellIds.size() << " cell IDs" << std::endl;
    std::cout << "  Cell now has " << cell->netIds.size() << " net IDs" << std::endl;
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

// Const version of getNetById
const Net* Netlist::getNetById(int id) const {
    if (id < 0 || id >= static_cast<int>(nets_.size())) {
        return nullptr;
    }
    // Ensure we return a const pointer when accessing via const reference
    return &nets_[id]; 
}

} // namespace fm 