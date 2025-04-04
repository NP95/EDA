#include "FMEngine.h"
#include <algorithm>
#include <random>
#include <chrono>

namespace fm {

FMEngine::FMEngine(Netlist& netlist, double balanceFactor)
    : netlist_(netlist)
    , partitionState_(netlist.getCells().size(), balanceFactor)
    , gainBucket_(getMaxPossibleDegree()) {
    initializePartitions();
}

void FMEngine::run() {
    bool improved;
    do {
        improved = runPass();
    } while (improved);
}

void FMEngine::initializePartitions() {
    // Create initial random partition
    std::vector<Cell>& cells = const_cast<std::vector<Cell>&>(netlist_.getCells());
    
    // Use current time as random seed
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::mt19937 gen(seed);
    std::uniform_int_distribution<> dis(0, 1);

    // Randomly assign cells to partitions
    int partition1Count = 0;
    int totalCells = cells.size();
    int targetSize = totalCells / 2;

    for (auto& cell : cells) {
        // Assign to partition 1 if we haven't reached target size
        if (partition1Count < targetSize) {
            cell.partition = 0;
            partition1Count++;
        } else {
            cell.partition = 1;
        }
        partitionState_.updatePartitionSize(cell.partition, 1);
    }

    // Update net partition counts and calculate initial cut size
    int cutSize = 0;
    for (auto& net : netlist_.getNets()) {
        net.partitionCount[0] = net.partitionCount[1] = 0;
        for (Cell* cell : net.cells) {
            net.partitionCount[cell->partition]++;
        }
        if (net.partitionCount[0] > 0 && net.partitionCount[1] > 0) {
            cutSize++;
        }
    }
    partitionState_.updateCutSize(cutSize);

    // Calculate initial gains and initialize gain bucket
    calculateInitialGains();
    gainBucket_.initialize(cells);
}

bool FMEngine::runPass() {
    const std::vector<Cell>& cells = netlist_.getCells();
    int numCells = cells.size();
    moveHistory_.clear();
    
    // Unlock all cells
    for (auto& cell : const_cast<std::vector<Cell>&>(cells)) {
        cell.locked = false;
    }

    int bestCutSize = partitionState_.getCurrentCutSize();
    int bestMoveIndex = -1;
    
    // Make numCells moves
    for (int i = 0; i < numCells; i++) {
        // Get highest gain cell that maintains balance
        Cell* cell = gainBucket_.getBestFeasibleCell(partitionState_);
        if (!cell) break;

        // Record move
        Move move;
        move.cell = cell;
        move.fromPartition = cell->partition;
        move.toPartition = 1 - cell->partition;
        move.gain = cell->gain;
        
        // Apply move
        applyMove(cell, move.toPartition);
        move.resultingCutSize = partitionState_.getCurrentCutSize();
        moveHistory_.push_back(move);

        // Update best solution if needed
        if (move.resultingCutSize < bestCutSize) {
            bestCutSize = move.resultingCutSize;
            bestMoveIndex = i;
        }
    }

    // If improvement found, keep moves up to best point
    if (bestMoveIndex >= 0) {
        revertMovesToBestState(bestMoveIndex);
        return true;
    } else {
        // No improvement, revert all moves
        revertMovesToBestState(-1);
        return false;
    }
}

void FMEngine::calculateInitialGains() {
    for (auto& cell : const_cast<std::vector<Cell>&>(netlist_.getCells())) {
        cell.gain = calculateCellGain(&cell);
    }
}

int FMEngine::calculateCellGain(Cell* cell) const {
    int gain = 0;
    int fromPartition = cell->partition;
    int toPartition = 1 - fromPartition;

    for (Net* net : cell->nets) {
        if (net->partitionCount[fromPartition] == 1) {
            // Moving cell would remove this net from cut
            gain--;
        }
        if (net->partitionCount[toPartition] == 0) {
            // Moving cell would add this net to cut
            gain++;
        }
    }

    return gain;
}

void FMEngine::updateGainsAfterMove(Cell* movedCell, int /*fromPartition*/) {
    // For each net connected to moved cell
    for (Net* net : movedCell->nets) {
        // For each cell on this net
        for (Cell* cell : net->cells) {
            if (cell != movedCell && !cell->locked) {
                int oldGain = cell->gain;
                int newGain = calculateCellGain(cell);
                
                if (oldGain != newGain) {
                    gainBucket_.updateCellGain(cell, oldGain, newGain);
                }
            }
        }
    }
}

void FMEngine::revertMovesToBestState(int bestMoveIndex) {
    // Revert moves from end to best state (or all moves if bestMoveIndex < 0)
    for (int i = moveHistory_.size() - 1; i > bestMoveIndex; i--) {
        const Move& move = moveHistory_[i];
        applyMove(move.cell, move.fromPartition);
    }
}

void FMEngine::applyMove(Cell* cell, int toPartition) {
    int fromPartition = cell->partition;
    
    // Update partition counts in nets
    for (Net* net : cell->nets) {
        net->partitionCount[fromPartition]--;
        net->partitionCount[toPartition]++;
        
        // Update cut size
        if (net->partitionCount[fromPartition] == 0) {
            partitionState_.updateCutSize(-1);  // Net no longer cut
        }
        if (net->partitionCount[toPartition] == 1) {
            partitionState_.updateCutSize(1);   // Net becomes cut
        }
    }

    // Update partition sizes
    partitionState_.updatePartitionSize(fromPartition, -1);
    partitionState_.updatePartitionSize(toPartition, 1);

    // Update cell's partition and lock it
    cell->partition = toPartition;
    cell->locked = true;

    // Remove from gain bucket (it's locked now)
    gainBucket_.removeCell(cell);

    // Update gains of affected cells
    updateGainsAfterMove(cell, fromPartition);
}

int FMEngine::getMaxPossibleDegree() const {
    int maxDegree = 0;
    for (const auto& cell : netlist_.getCells()) {
        maxDegree = std::max(maxDegree, static_cast<int>(cell.nets.size()));
    }
    return maxDegree;
}

bool FMEngine::isMoveLegal(Cell* cell, int toPartition) const {
    if (!cell || cell->locked) return false;

    int fromPartition = cell->partition;
    if (fromPartition == toPartition) return false;

    // Check if move maintains balance
    int newSize1 = partitionState_.getPartitionSize(fromPartition) - 1;
    int newSize2 = partitionState_.getPartitionSize(toPartition) + 1;
    
    return partitionState_.isBalanced(newSize1, newSize2);
}

} // namespace fm 