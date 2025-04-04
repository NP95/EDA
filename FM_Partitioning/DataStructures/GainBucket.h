#pragma once

#include <vector>
#include "Netlist.h"
#include "PartitionState.h"

namespace fm {

struct BucketNode {
    Cell* cellPtr = nullptr;    // Pointer back to the cell
    BucketNode *prev = nullptr, *next = nullptr;
};

class GainBucket {
public:
    // Constructor/Destructor
    GainBucket(int maxPossibleDegree);
    ~GainBucket();

    // Bucket operations
    void initialize(const std::vector<Cell>& cells);
    void addCell(Cell* cell);
    void removeCell(Cell* cell);
    void updateCellGain(Cell* cell, int oldGain, int newGain);
    Cell* getBestFeasibleCell(const PartitionState& state);
    
    // Accessors
    int getMaxGain(int partition) const { return maxGain_[partition]; }

private:
    std::vector<BucketNode*> buckets_[2];  // Array of lists for G1 and G2
    int maxGain_[2] = {0, 0};              // Tracks highest gain in each partition
    int maxPossibleDegree;                 // Maximum possible degree (for gain indexing)
    
    // Helper methods
    int gainToIndex(int gain) const;
    void updateMaxGain(int partition);
    BucketNode* createNode(Cell* cell);
};

} // namespace fm 