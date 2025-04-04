#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace fm {

// Forward declarations
struct Net;
struct BucketNode;

struct Cell {
    std::string name;
    int id;                     // Unique integer ID
    int partition = 0;          // e.g., 0 for G1, 1 for G2
    int gain = 0;
    bool locked = false;
    std::vector<Net*> nets;     // Pointers to connected nets
    BucketNode* bucketNodePtr = nullptr; // Pointer to node in GainBucket's linked list
};

struct Net {
    std::string name;
    int id;                     // Unique integer ID
    std::vector<Cell*> cells;   // Pointers to connected cells
    int partitionCount[2] = {0, 0}; // Count of cells in G1 (index 0) and G2 (index 1)
};

class Netlist {
public:
    // Constructor/Destructor
    Netlist() = default;
    ~Netlist() = default;

    // Cell management
    void addCell(const std::string& name);
    Cell* getCellByName(const std::string& name);
    Cell* getCellById(int id);

    // Net management
    void addNet(const std::string& name);
    void addCellToNet(const std::string& netName, const std::string& cellName);
    Net* getNetByName(const std::string& name);
    Net* getNetById(int id);

    // Accessors
    const std::vector<Cell>& getCells() const { return cells_; }
    std::vector<Net>& getNets() { return nets_; }

private:
    std::vector<Cell> cells_;
    std::vector<Net> nets_;
    std::unordered_map<std::string, int> cellNameToId_;
    std::unordered_map<std::string, int> netNameToId_;
};

} // namespace fm 