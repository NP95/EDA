#pragma once

namespace fm {

class PartitionState {
public:
    // Constructor
    PartitionState(int totalCells, double balanceFactor);

    // State management
    bool isBalanced(int partition1Size, int partition2Size) const;
    void updateCutSize(int delta);
    void updatePartitionSize(int partition, int delta);

    // Accessors
    int getCurrentCutSize() const { return currentCutsize; }
    int getPartitionSize(int partition) const { return partitionSize[partition]; }
    int getMinPartitionSize() const { return minPartitionSize; }
    int getMaxPartitionSize() const { return maxPartitionSize; }
    double getBalanceFactor() const { return balanceFactor; }
    int getTotalCells() const { return totalCells; }

private:
    int partitionSize[2] = {0, 0};  // Current number of cells in G1 and G2
    int currentCutsize = 0;         // Current cut size
    double balanceFactor;           // The required r
    int totalCells;                 // Total number of cells n
    int minPartitionSize;           // Pre-calculated balance limits
    int maxPartitionSize;           // Pre-calculated balance limits

    void calculateBalanceLimits();
};

} // namespace fm 