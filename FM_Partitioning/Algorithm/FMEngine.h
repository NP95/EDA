#pragma once

#include "../DataStructures/Netlist.h"
#include "../DataStructures/PartitionState.h"
#include "../DataStructures/GainBucket.h"
#include <vector>

namespace fm {

struct Move {
    Cell* cell;
    int fromPartition;
    int toPartition;
    int gain;
    int resultingCutSize;
};

class FMEngine {
public:
    // Constructor
    FMEngine(Netlist& netlist, double balanceFactor);

    // Main algorithm methods
    void run();
    
    // Accessor for partition state
    const PartitionState& getPartitionState() const { return partitionState_; }
    
private:
    Netlist& netlist_;
    PartitionState partitionState_;
    GainBucket gainBucket_;
    std::vector<Move> moveHistory_;

    // Core algorithm steps
    void initializePartitions();
    bool runPass();
    
    // Helper methods
    void calculateInitialGains();
    void updateGainsAfterMove(Cell* movedCell, int fromPartition);
    int calculateCellGain(Cell* cell) const;
    void revertMovesToBestState(int bestMoveIndex);
    void applyMove(Cell* cell, int toPartition);
    
    // Utility methods
    int getMaxPossibleDegree() const;
    bool isMoveLegal(Cell* cell, int toPartition) const;
};

} // namespace fm 