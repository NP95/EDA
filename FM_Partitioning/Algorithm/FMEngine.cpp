#include "FMEngine.h"
#include <algorithm>
#include <random>
#include <chrono>
#include <iostream>
#include <unordered_set>

namespace fm {

FMEngine::FMEngine(Netlist& netlist, double balanceFactor)
    : netlist_(netlist)
    , partitionState_(netlist.getCells().size(), balanceFactor)
    , gainBucket_(getMaxPossibleDegree()) {
    std::cout << "Initializing FMEngine..." << std::endl;
    initializePartitions();
    std::cout << "FMEngine initialized." << std::endl;
}

void FMEngine::run() {
    std::cout << "Starting F-M passes..." << std::endl;
    bool improved;
    int passCount = 0;
    const int MAX_PASSES = 50;  // Limit maximum number of passes
    int lastCutSize = partitionState_.getCurrentCutSize();
    int noImprovementCount = 0;
    const int MAX_NO_IMPROVEMENT = 3;  // Stop if no improvement for this many passes

    std::cout << "Initial state - Cut size: " << lastCutSize 
              << ", Partition sizes: [" << partitionState_.getPartitionSize(0) 
              << ", " << partitionState_.getPartitionSize(1) << "]" << std::endl;

    do {
        passCount++;
        std::cout << "\nStarting pass " << passCount << " of maximum " << MAX_PASSES << std::endl;
        
        // Verify state before pass
        if (!partitionState_.isBalanced(partitionState_.getPartitionSize(0), 
                                      partitionState_.getPartitionSize(1))) {
            std::cerr << "Error: Unbalanced partitions before pass " << passCount << std::endl;
            break;
        }

        // Run the pass, passing the current pass number
        improved = runPass(passCount);
        int currentCutSize = partitionState_.getCurrentCutSize();
        
        std::cout << "Pass " << passCount << " completed. "
                  << "Previous cut size: " << lastCutSize 
                  << ", Current cut size: " << currentCutSize 
                  << ", Improved: " << (improved ? "yes" : "no") << std::endl;
        
        // Check for actual improvement in cut size
        if (currentCutSize >= lastCutSize) {
            noImprovementCount++;
            std::cout << "No cut size improvement for " << noImprovementCount 
                      << " passes" << std::endl;
            
            if (noImprovementCount >= MAX_NO_IMPROVEMENT) {
                std::cout << "Stopping due to lack of improvement for " 
                          << MAX_NO_IMPROVEMENT << " passes" << std::endl;
                break;
            }
        } else {
            noImprovementCount = 0;
            std::cout << "Cut size improved by " << (lastCutSize - currentCutSize) 
                      << std::endl;
        }
        
        lastCutSize = currentCutSize;

        // Verify state after pass
        if (!partitionState_.isBalanced(partitionState_.getPartitionSize(0), 
                                      partitionState_.getPartitionSize(1))) {
            std::cerr << "Error: Unbalanced partitions after pass " << passCount << std::endl;
            break;
        }

        // Check for maximum passes
        if (passCount >= MAX_PASSES) {
            std::cout << "Stopping due to maximum pass limit (" << MAX_PASSES 
                      << ") reached" << std::endl;
            break;
        }

        // Verify all cells are unlocked between passes
        bool allUnlocked = true;
        for (const auto& cell : netlist_.getCells()) {
            if (cell.locked) {
                std::cerr << "Error: Cell " << cell.name << " still locked after pass" << std::endl;
                allUnlocked = false;
            }
        }
        if (!allUnlocked) {
            std::cerr << "Error: Some cells still locked between passes" << std::endl;
            break;
        }

    } while (improved);

    std::cout << "F-M passes completed after " << passCount << " passes." << std::endl;
    std::cout << "Final state - Cut size: " << partitionState_.getCurrentCutSize() 
              << ", Partition sizes: [" << partitionState_.getPartitionSize(0) 
              << ", " << partitionState_.getPartitionSize(1) << "]" << std::endl;
}

void FMEngine::initializePartitions() {
    std::cout << "Creating initial partition..." << std::endl;
    // Get references to cells and nets
    std::vector<Cell>& cells = const_cast<std::vector<Cell>&>(netlist_.getCells());
    std::vector<Net>& nets = netlist_.getNets(); // Use the getter that returns a non-const reference
    
    // Validate cells vector
    if (cells.empty()) {
        std::cerr << "Error: No cells found in netlist" << std::endl;
        return;
    }

    // Reset all cell partitions (but preserve connectivity)
    for (auto& cell : cells) {
        cell.partition = -1; // Mark as unassigned
        cell.gain = 0;       // Reset gain
        cell.locked = false; // Make sure cell is unlocked
    }
    
    // Reset net partition counts 
    for (auto& net : nets) {
        net.partitionCount[0] = 0;
        net.partitionCount[1] = 0;
    }
    
    // Create balanced initial partition
    int totalCells = cells.size();
    int targetSize = totalCells / 2;
    int partition1Count = 0;
    
    // Assign cells to partitions sequentially for deterministic results
    for (auto& cell : cells) {
        // Assign to partition 0 if we haven't reached target size
        if (partition1Count < targetSize) {
            cell.partition = 0;
            partition1Count++;
        } else {
            cell.partition = 1;
        }
        
        // Update partition counts for all nets this cell belongs to
        // Iterate over net IDs now
        for (int netId : cell.netIds) {
            Net* net = netlist_.getNetById(netId); // Fetch net by ID
            if (net) {
                net->partitionCount[cell.partition]++;
            } else {
                 std::cerr << "Warning: Net with ID " << netId << " not found during init for cell " << cell.name << std::endl;
            }
        }
    }
    
    std::cout << "Initial partition created." << std::endl;
    std::cout << "Partition sizes: [" << partition1Count << ", " 
              << (totalCells - partition1Count) << "]" << std::endl;
    
    // Update partition state
    partitionState_.updatePartitionSize(0, partition1Count);
    partitionState_.updatePartitionSize(1, totalCells - partition1Count);
    
    // Calculate initial cut size
    int initialCutSize = 0;
    // Use the nets vector directly
    for (const auto& net : nets) { 
        // A net is cut if it has cells in both partitions
        if (net.partitionCount[0] > 0 && net.partitionCount[1] > 0) {
            initialCutSize++;
        }
    }
    
    // Set the initial cut size
    partitionState_.updateCutSize(initialCutSize);
    std::cout << "Initial cut size: " << initialCutSize << std::endl;
    
    // Calculate initial cell gains
    calculateInitialGains();
    
    // Initialize the gain bucket with all cells
    gainBucket_.initialize(cells);
    
    std::cout << "FMEngine initialization completed." << std::endl;
}

bool FMEngine::runPass(int passCount) {
    // std::cout << "Starting runPass..." << std::endl; // Reduced logging
    const std::vector<Cell>& cells = netlist_.getCells();
    int numCells = cells.size();
    moveHistory_.clear();
    int initialCutSize = partitionState_.getCurrentCutSize(); // Store initial cutsize

    // std::cout << "Initial state - Cut size: " << initialCutSize \n    //           << ", Partition sizes: [" << partitionState_.getPartitionSize(0) \n    //           << ", " << partitionState_.getPartitionSize(1) << "]" << std::endl; // Reduced logging

    // Unlock all cells and verify gain bucket state
    // std::cout << "Unlocking cells and verifying gain bucket..." << std::endl; // Reduced logging
    for (auto& cell : const_cast<std::vector<Cell>&>(cells)) {
        cell.locked = false;
        // Verify cell's gain matches its position in gain bucket
        if (cell.bucketNodePtr && cell.bucketNodePtr->cellPtr != &cell) {
            // This check might still be useful, but let's silence it for perf
            // std::cerr << "Error: Cell " << cell.name << " has inconsistent bucket node pointer" << std::endl;
            gainBucket_.removeCell(&cell);
            gainBucket_.addCell(&cell);
        }
    }

    int bestCutSize = partitionState_.getCurrentCutSize();
    int bestMoveIndex = -1;
    int movesWithoutImprovement = 0;

    // --- Adaptive Threshold Calculation --- START
    const int START_THRESHOLD = 2000;
    const int MIN_THRESHOLD = 500;
    const int DECREASE_PER_PASS = 100;
    int adaptiveThreshold = std::max(MIN_THRESHOLD, START_THRESHOLD - (passCount - 1) * DECREASE_PER_PASS);
    // const int MAX_MOVES_WITHOUT_IMPROVEMENT = 1000; // Limit unproductive moves - OLD FIXED
    const int MAX_MOVES_WITHOUT_IMPROVEMENT = adaptiveThreshold; // NEW ADAPTIVE
    // --- Adaptive Threshold Calculation --- END

    // Track moved cells to prevent infinite loops
    std::unordered_set<Cell*> movedCells;

    // std::cout << "Starting moves loop..." << std::endl; // Reduced logging
    // Make moves until we can't improve or reach all cells
    for (int i = 0; i < numCells && movesWithoutImprovement < MAX_MOVES_WITHOUT_IMPROVEMENT; i++) {
        // std::cout << "\nMove " << i + 1 << " of " << numCells << std::endl; // Reduced logging
        // std::cout << "Current cut size: " << partitionState_.getCurrentCutSize() << std::endl; // Reduced logging

        // Get highest gain cell that maintains balance
        Cell* cell = gainBucket_.getBestFeasibleCell(partitionState_);
        if (!cell) {
            // std::cout << "No feasible cell found, breaking..." << std::endl; // Reduced logging
            break;
        }

        // Check if cell was already moved
        if (movedCells.find(cell) != movedCells.end()) {
            // std::cout << "Cell " << cell->name << " was already moved in this pass, breaking..." << std::endl; // Reduced logging
            break;
        }

        // Validate selected cell
        if (cell->locked) {
            std::cerr << "Error: Selected locked cell " << cell->name << std::endl; // Keep critical errors
            break;
        }

        // std::cout << "Selected cell " << cell->name << " with gain " << cell->gain \n        //           << " from partition " << cell->partition << std::endl; // Reduced logging

        // Record move
        Move move;
        move.cell = cell;
        move.fromPartition = cell->partition;
        move.toPartition = 1 - cell->partition;
        move.gain = cell->gain;

        // Verify move legality
        if (!isMoveLegal(cell, move.toPartition)) {
            std::cerr << "Error: Illegal move detected for cell " << cell->name << std::endl; // Keep critical errors
            break;
        }

        // Apply move
        // std::cout << "Applying move..." << std::endl; // Reduced logging
        applyMove(cell, move.toPartition);
        move.resultingCutSize = partitionState_.getCurrentCutSize();
        moveHistory_.push_back(move);
        movedCells.insert(cell);  // Track that this cell was moved

        // std::cout << "Move completed. New cut size: " << move.resultingCutSize \n        //           << ", Partition sizes: [" << partitionState_.getPartitionSize(0) \n        //           << ", " << partitionState_.getPartitionSize(1) << "]" << std::endl; // Reduced logging

        // Update best solution if improved
        if (move.resultingCutSize < bestCutSize) {
            bestCutSize = move.resultingCutSize;
            bestMoveIndex = i;
            movesWithoutImprovement = 0;
            // std::cout << "New best solution found at move " << i \n            //           << " with cut size " << bestCutSize << std::endl; // Reduced logging
        } else {
            movesWithoutImprovement++;
            // std::cout << "No improvement for " << movesWithoutImprovement \n            //           << " moves" << std::endl; // Reduced logging
        }

        // Verify partition balance after move
        if (!partitionState_.isBalanced(partitionState_.getPartitionSize(0),
                                      partitionState_.getPartitionSize(1))) {
            std::cerr << "Error: Unbalanced partitions after move" << std::endl; // Keep critical errors
            break;
        }

        // Verify all cells in gain bucket are unlocked
        // This check might be slow, consider removing if confident
        /*
        bool foundLockedCell = false;
        for (const auto& c : cells) {
            if (c.locked && c.bucketNodePtr != nullptr) {
                std::cerr << "Error: Locked cell " << c.name << " found in gain bucket" << std::endl; // Keep critical errors
                foundLockedCell = true;
            }
        }
        if (foundLockedCell) {
            break;
        }
        */
    }

    // std::cout << "\nMoves completed. Best move index: " << bestMoveIndex << std::endl; // Reduced logging

    // Revert moves based on the best state found
    revertMovesToBestState(bestMoveIndex, initialCutSize); // Pass initial cutsize

    // Return true if an improvement was potentially found and kept
    return bestMoveIndex >= 0 && bestCutSize < initialCutSize;
}

void FMEngine::calculateInitialGains() {
    for (auto& cell : const_cast<std::vector<Cell>&>(netlist_.getCells())) {
        cell.gain = calculateCellGain(&cell);
    }
}

int FMEngine::calculateCellGain(Cell* cell) const {
    if (!cell) {
        std::cerr << "calculateCellGain: Error - null cell pointer" << std::endl;
        return 0;
    }

    int gain = 0;
    int fromPartition = cell->partition;
    int toPartition = 1 - fromPartition;

    // Iterate over net IDs
    for (int netId : cell->netIds) {
        const Net* net = netlist_.getNetById(netId); // Fetch net by ID (const version if possible, check Netlist)
        if (!net) {
            std::cerr << "Warning: Net with ID " << netId << " not found during gain calc for cell " << cell->name << std::endl;
            continue; 
        }

        // Check net distribution
        int fromCount = net->partitionCount[fromPartition];
        int toCount = net->partitionCount[toPartition];

        // If moving this cell makes the net uncut (FS=1, TE=0 -> FS=0, TE=1)
        if (fromCount == 1 && toCount == 0) {
            gain++;
        }
        // If moving this cell makes the net cut (FS=current, TE=0 -> FS=current-1, TE=1)
        else if (toCount == 0) {
            gain--;
        }
    }
    return gain;
}

void FMEngine::updateGainsAfterMove(Cell* movedCell) {
    if (!movedCell) {
        std::cerr << "updateGainsAfterMove: Error - null cell pointer" << std::endl;
        return;
    }

    // Track affected cells that need gain updates
    std::unordered_set<Cell*> cellsToUpdate;
    
    // Process each net connected to the moved cell
    for (int netId : movedCell->netIds) {
        Net* net = netlist_.getNetById(netId); // Fetch net by ID
        if (!net) continue;
        
        // Get all cell IDs on this net
        for (int neighborCellId : net->cellIds) {
            Cell* neighborCell = netlist_.getCellById(neighborCellId); // Fetch neighbor cell
            if (!neighborCell) continue;

            // Skip the moved cell and locked cells
            if (neighborCell == movedCell || neighborCell->locked) continue;
            
            // Add to update list
            cellsToUpdate.insert(neighborCell);
        }
    }
    
    // Update gains for all affected cells
    for (Cell* cell : cellsToUpdate) {
        int oldGain = cell->gain;
        int newGain = calculateCellGain(cell);
        
        if (oldGain != newGain) {
            gainBucket_.updateCellGain(cell, oldGain, newGain);
        }
    }
}

void FMEngine::revertMovesToBestState(int bestMoveIndex, int initialCutSize) {
    // Revert moves from end to best state (or all moves if bestMoveIndex < 0)
    std::cout << "Reverting moves from index " << moveHistory_.size() - 1 << " down to " << bestMoveIndex + 1 << std::endl;
    for (int i = moveHistory_.size() - 1; i > bestMoveIndex; i--) {
        const Move& move = moveHistory_[i];
        std::cout << "  Reverting move " << i << " for cell " << move.cell->name << std::endl;
        undoMove(move);
    }
    std::cout << "Move reversion complete." << std::endl;
    
    // Clear the rest of the move history (moves that were kept or reverted)
    if (bestMoveIndex + 1 < static_cast<int>(moveHistory_.size())) {
        moveHistory_.resize(bestMoveIndex + 1);
    } else if (bestMoveIndex == -1) {
        moveHistory_.clear(); // Cleared all moves
        // Explicitly reset cutsize to the state before the pass began
        partitionState_.setCurrentCutSize(initialCutSize);
        // Debug: Immediately check the value after setting
        std::cout << "Debug: Cut size immediately after reset: " << partitionState_.getCurrentCutSize() << std::endl; 
        std::cout << "All moves reverted. Cut size reset to initial pass value: " << initialCutSize << std::endl;
    }

    // Recalculate the cut size based on the current state after reverts
    int finalCutSize = calculateCurrentCutSize();
    partitionState_.setCurrentCutSize(finalCutSize);
    std::cout << "Cut size recalculated after reverts: " << finalCutSize << std::endl;

    // Recalculate all gains and rebuild bucket for consistency after reverting?
    // Maybe not necessary if undoMove handles gains correctly. But could be a safeguard.
    // Let's skip this for now and rely on undoMove.
    
     std::cout << "State after reversion - Cut size: " << partitionState_.getCurrentCutSize() 
              << ", Partition sizes: [" << partitionState_.getPartitionSize(0) 
              << ", " << partitionState_.getPartitionSize(1) << "]" << std::endl;
}

void FMEngine::applyMove(Cell* cell, int toPartition) {
    if (!cell) {
        std::cerr << "applyMove: Error - null cell pointer" << std::endl;
        return;
    }

    int fromPartition = cell->partition;
    if (fromPartition == toPartition) {
        return; // No change needed
    }

    // Calculate cutsize delta directly
    int cutsizeDelta = -cell->gain; // Gain is the negative of cutsize change
    
    // Remove cell from gain bucket and lock it
    gainBucket_.removeCell(cell);
    cell->locked = true;
    
    // Update partition sizes
    partitionState_.updatePartitionSize(fromPartition, -1);
    partitionState_.updatePartitionSize(toPartition, 1);
    
    // Update net partition counts
    for (int netId : cell->netIds) {
        Net* net = netlist_.getNetById(netId); // Fetch net by ID
        if (!net) continue;
        
        // Decrement count in old partition
        net->partitionCount[fromPartition]--;
        // Increment count in new partition
        net->partitionCount[toPartition]++;
    }
    
    // Update cell's partition
    cell->partition = toPartition;
    
    // Update cutsize
    partitionState_.updateCutSize(cutsizeDelta);

    // Update gains of neighbors AFTER partition counts and cell partition are updated
    updateGainsAfterMove(cell);

    std::cout << "    Move applied. New cut size: " << partitionState_.getCurrentCutSize() 
              << ", Partition sizes: [" << partitionState_.getPartitionSize(0) 
              << ", " << partitionState_.getPartitionSize(1) << "]" << std::endl;
}

int FMEngine::getMaxPossibleDegree() const {
    // Find the cell with the maximum number of connected nets
    int maxDegree = 0;
    
    for (const auto& cell : netlist_.getCells()) {
        int degree = cell.netIds.size();
        maxDegree = std::max(maxDegree, degree);
    }
    
    // If empty netlist, return a minimum size
    if (maxDegree == 0) {
        return 10; // Default minimum size
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

void FMEngine::undoMove(const Move& move) {
    Cell* cell = move.cell;
    int originalPartition = move.fromPartition;
    int movedToPartition = move.toPartition;
    std::cout << "    Undoing move of cell " << cell->name << " from " << movedToPartition << " to " << originalPartition << std::endl;

    // 1. Calculate the cutsize change to undo.
    //    The original move changed cutsize by -move.gain (where move.gain was the gain *before* the move).
    //    To reverse this, we add +move.gain.
    int cutsizeDeltaUndo = move.gain;

    // 2. Update partition sizes (reverse of applyMove)
    partitionState_.updatePartitionSize(movedToPartition, -1);
    partitionState_.updatePartitionSize(originalPartition, 1);

    // 3. Update net partition counts (reverse of applyMove)
    //    Do this BEFORE changing cell->partition
    for (int netId : cell->netIds) {
        Net* net = netlist_.getNetById(netId);
        if (!net) continue;
        net->partitionCount[movedToPartition]--;
        net->partitionCount[originalPartition]++;
    }

    // 4. Restore cell's partition
    cell->partition = originalPartition;
    
    // 5. Unlock the cell
    cell->locked = false;

    // 6. Update gains of neighbors based on the now reverted state of 'cell' and nets
    //    Neighbors' gains might change due to 'cell' moving back
    updateGainsAfterMove(cell);

    // 7. Recalculate the gain of the moved cell itself in its original partition
    int currentGain = calculateCellGain(cell); 
    cell->gain = currentGain;

    // 8. Add the cell back to the gain bucket with its recalculated gain
    gainBucket_.addCell(cell); 

    // 9. Apply the cutsize reversal *after* state is restored
    partitionState_.updateCutSize(cutsizeDeltaUndo); 
    
    std::cout << "    Undo complete. Cut size after undo: " << partitionState_.getCurrentCutSize() << std::endl;
}

int FMEngine::calculateCurrentCutSize() const {
    int currentCutSize = 0;
    for (const auto& net : netlist_.getNets()) {
        if (net.partitionCount[0] > 0 && net.partitionCount[1] > 0) {
            currentCutSize++;
        }
    }
    return currentCutSize;
}

} // namespace fm 