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

        // Run the pass
        improved = runPass();
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
    std::vector<Net>& nets = const_cast<std::vector<Net>&>(netlist_.getNets());
    
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
        for (Net* net : cell.nets) {
            if (net) {
                net->partitionCount[cell.partition]++;
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

bool FMEngine::runPass() {
    std::cout << "Starting runPass..." << std::endl;
    const std::vector<Cell>& cells = netlist_.getCells();
    int numCells = cells.size();
    moveHistory_.clear();
    
    std::cout << "Initial state - Cut size: " << partitionState_.getCurrentCutSize() 
              << ", Partition sizes: [" << partitionState_.getPartitionSize(0) 
              << ", " << partitionState_.getPartitionSize(1) << "]" << std::endl;
    
    // Unlock all cells and verify gain bucket state
    std::cout << "Unlocking cells and verifying gain bucket..." << std::endl;
    for (auto& cell : const_cast<std::vector<Cell>&>(cells)) {
        cell.locked = false;
        // Verify cell's gain matches its position in gain bucket
        if (cell.bucketNodePtr && cell.bucketNodePtr->cellPtr != &cell) {
            std::cerr << "Error: Cell " << cell.name << " has inconsistent bucket node pointer" << std::endl;
            gainBucket_.removeCell(&cell);
            gainBucket_.addCell(&cell);
        }
    }

    int bestCutSize = partitionState_.getCurrentCutSize();
    int bestMoveIndex = -1;
    int movesWithoutImprovement = 0;
    const int MAX_MOVES_WITHOUT_IMPROVEMENT = numCells / 2;  // Limit unproductive moves
    
    // Track moved cells to prevent infinite loops
    std::unordered_set<Cell*> movedCells;
    
    std::cout << "Starting moves loop..." << std::endl;
    // Make moves until we can't improve or reach all cells
    for (int i = 0; i < numCells && movesWithoutImprovement < MAX_MOVES_WITHOUT_IMPROVEMENT; i++) {
        std::cout << "\nMove " << i + 1 << " of " << numCells << std::endl;
        std::cout << "Current cut size: " << partitionState_.getCurrentCutSize() << std::endl;
        
        // Get highest gain cell that maintains balance
        Cell* cell = gainBucket_.getBestFeasibleCell(partitionState_);
        if (!cell) {
            std::cout << "No feasible cell found, breaking..." << std::endl;
            break;
        }

        // Check if cell was already moved
        if (movedCells.find(cell) != movedCells.end()) {
            std::cout << "Cell " << cell->name << " was already moved in this pass, breaking..." << std::endl;
            break;
        }

        // Validate selected cell
        if (cell->locked) {
            std::cerr << "Error: Selected locked cell " << cell->name << std::endl;
            break;
        }

        std::cout << "Selected cell " << cell->name << " with gain " << cell->gain 
                  << " from partition " << cell->partition << std::endl;

        // Record move
        Move move;
        move.cell = cell;
        move.fromPartition = cell->partition;
        move.toPartition = 1 - cell->partition;
        move.gain = cell->gain;
        
        // Verify move legality
        if (!isMoveLegal(cell, move.toPartition)) {
            std::cerr << "Error: Illegal move detected for cell " << cell->name << std::endl;
            break;
        }
        
        // Apply move
        std::cout << "Applying move..." << std::endl;
        applyMove(cell, move.toPartition);
        move.resultingCutSize = partitionState_.getCurrentCutSize();
        moveHistory_.push_back(move);
        movedCells.insert(cell);  // Track that this cell was moved

        std::cout << "Move completed. New cut size: " << move.resultingCutSize 
                  << ", Partition sizes: [" << partitionState_.getPartitionSize(0) 
                  << ", " << partitionState_.getPartitionSize(1) << "]" << std::endl;

        // Update best solution if improved
        if (move.resultingCutSize < bestCutSize) {
            bestCutSize = move.resultingCutSize;
            bestMoveIndex = i;
            movesWithoutImprovement = 0;
            std::cout << "New best solution found at move " << i 
                      << " with cut size " << bestCutSize << std::endl;
        } else {
            movesWithoutImprovement++;
            std::cout << "No improvement for " << movesWithoutImprovement 
                      << " moves" << std::endl;
        }

        // Verify partition balance after move
        if (!partitionState_.isBalanced(partitionState_.getPartitionSize(0), 
                                      partitionState_.getPartitionSize(1))) {
            std::cerr << "Error: Unbalanced partitions after move" << std::endl;
            break;
        }

        // Verify all cells in gain bucket are unlocked
        bool foundLockedCell = false;
        for (const auto& c : cells) {
            if (c.locked && c.bucketNodePtr != nullptr) {
                std::cerr << "Error: Locked cell " << c.name << " found in gain bucket" << std::endl;
                foundLockedCell = true;
            }
        }
        if (foundLockedCell) {
            break;
        }
    }

    std::cout << "\nMoves completed. Best move index: " << bestMoveIndex << std::endl;

    // If improvement found, keep moves up to best point
    if (bestMoveIndex >= 0) {
        std::cout << "Reverting to best state..." << std::endl;
        revertMovesToBestState(bestMoveIndex);
        
        // Verify final state
        std::cout << "Final state - Cut size: " << partitionState_.getCurrentCutSize() 
                  << ", Partition sizes: [" << partitionState_.getPartitionSize(0) 
                  << ", " << partitionState_.getPartitionSize(1) << "]" << std::endl;
        
        if (!partitionState_.isBalanced(partitionState_.getPartitionSize(0), 
                                      partitionState_.getPartitionSize(1))) {
            std::cerr << "Error: Unbalanced final state" << std::endl;
            return false;
        }
        
        return true;
    } else {
        std::cout << "No improvement found, reverting all moves..." << std::endl;
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
    if (!cell) {
        std::cerr << "calculateCellGain: Error - null cell pointer" << std::endl;
        return 0;
    }

    int gain = 0;
    int fromPartition = cell->partition;
    int toPartition = 1 - fromPartition;

    for (Net* net : cell->nets) {
        if (!net) {
            std::cerr << "calculateCellGain: Error - found null net pointer for cell " 
                     << cell->name << std::endl;
            continue;
        }

        // Gain calculation based on FM algorithm:
        // 1. If this is the only cell in fromPartition, moving it would make the net uncut
        if (net->partitionCount[fromPartition] == 1) {
            gain++; // Net becomes uncut (reduces cutsize) -> positive gain
        }
        
        // 2. If there are no cells in toPartition, moving this cell would make the net cut
        if (net->partitionCount[toPartition] == 0) {
            gain--; // Net becomes cut (increases cutsize) -> negative gain
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
    for (Net* net : movedCell->nets) {
        if (!net) continue;
        
        // Get all cells on this net
        for (Cell* neighborCell : net->cells) {
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

void FMEngine::revertMovesToBestState(int bestMoveIndex) {
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
    }

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
    for (Net* net : cell->nets) {
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
    
    // Update gains of affected cells
    updateGainsAfterMove(cell);
}

int FMEngine::getMaxPossibleDegree() const {
    // Find the cell with the maximum number of connected nets
    int maxDegree = 0;
    
    for (const auto& cell : netlist_.getCells()) {
        int degree = cell.nets.size();
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

// Create a new function to properly undo a move
void FMEngine::undoMove(const Move& move) {
    if (!move.cell) {
        std::cerr << "undoMove: Error - null cell pointer in move" << std::endl;
        return;
    }

    Cell* cell = move.cell;
    int currentPartition = cell->partition; // Should be move.toPartition
    int originalPartition = move.fromPartition; // The partition to return to

    std::cout << "Undoing move for cell " << cell->name 
              << " from partition " << currentPartition 
              << " back to partition " << originalPartition << std::endl;

    if (currentPartition != move.toPartition) {
         std::cerr << "undoMove: Error - cell " << cell->name 
                   << " is in unexpected partition " << currentPartition 
                   << " (expected " << move.toPartition << ")" << std::endl;
        // Attempt to proceed anyway, but log the warning
    }
    if (currentPartition == originalPartition) {
        std::cerr << "undoMove: Error - attempting to undo move to the same partition" << std::endl;
        return;
    }
    
    // NOTE: We DO NOT remove the cell from the gain bucket here, as it was already
    // removed during applyMove. Gain updates will handle re-adding it later.

    // Update partition counts for all nets connected to this cell
    for (Net* net : cell->nets) {
        if (!net) {
            std::cerr << "undoMove: Error - found null net pointer for cell " << cell->name << std::endl;
            continue;
        }

        // Store old partition counts for validation
        int oldCount0 = net->partitionCount[0];
        int oldCount1 = net->partitionCount[1];

        // Validate current partition counts
        if (oldCount0 < 0 || oldCount1 < 0) {
            std::cerr << "undoMove: Error - negative partition count for net " << net->name 
                     << " [" << oldCount0 << ", " << oldCount1 << "]" << std::endl;
            continue;
        }

        // Update partition counts (reverse of applyMove)
        net->partitionCount[currentPartition]--;
        net->partitionCount[originalPartition]++;

        // Validate new partition counts
        if (net->partitionCount[currentPartition] < 0) {
            std::cerr << "undoMove: Error - negative partition count after undo for net " << net->name 
                     << " partition " << currentPartition << std::endl;
            // Revert the change
            net->partitionCount[currentPartition]++;
            net->partitionCount[originalPartition]--;
            continue;
        }

        // Verify total cell count hasn't changed
        int totalCount = net->partitionCount[0] + net->partitionCount[1];
         if (totalCount != static_cast<int>(net->cells.size())) {
            std::cerr << "undoMove: Error - partition count mismatch for net " << net->name 
                     << ". Expected " << net->cells.size() 
                     << ", got " << totalCount << std::endl;
        }


        // Update cut size based on net state change
        bool wasCut = (oldCount0 > 0 && oldCount1 > 0);
        bool isCut = (net->partitionCount[0] > 0 && net->partitionCount[1] > 0);
        
        if (wasCut && !isCut) {
            partitionState_.updateCutSize(-1);
             std::cout << "  Net " << net->name << " is no longer cut (undo)" << std::endl;
        } else if (!wasCut && isCut) {
            partitionState_.updateCutSize(1);
            std::cout << "  Net " << net->name << " is now cut (undo)" << std::endl;
        }
         std::cout << "  Updated net " << net->name << " partition counts: [" 
                  << net->partitionCount[0] << ", " << net->partitionCount[1] << "] (undo)" << std::endl;
    }

    // Update partition sizes
    partitionState_.updatePartitionSize(currentPartition, -1);
    partitionState_.updatePartitionSize(originalPartition, 1);

    // Update cell's partition
    cell->partition = originalPartition;
    cell->locked = false; // Unlock the cell when undoing the move

    // Update gains of affected cells (neighbors)
    // Note: The gain of the moved cell itself doesn't need explicit update here.
    // Its gain will be recalculated if it's selected again or at the start of the next pass.
    // The gain update for neighbors is crucial.
    updateGainsAfterMove(cell);

    // Re-add the cell to the gain bucket now that its state is correct
    gainBucket_.addCell(cell); // Should use the cell's current (reverted) gain

     std::cout << "Undo move completed. Current cut size: " << partitionState_.getCurrentCutSize() << std::endl;
}

} // namespace fm 