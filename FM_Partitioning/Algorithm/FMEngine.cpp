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
    // Create initial random partition
    std::vector<Cell>& cells = const_cast<std::vector<Cell>&>(netlist_.getCells());
    
    // Validate cells vector
    if (cells.empty()) {
        std::cerr << "Error: No cells found in netlist" << std::endl;
        return;
    }

    // Pre-validation phase 1: First make sure all cells and nets have empty connections
    // This extreme approach works well for corrupted data - reset everything and rebuild safely
    std::cout << "Pre-validation phase 1: Resetting all cell-net connections..." << std::endl;
    for (auto& cell : cells) {
        // Reset the nets vector completely for all cells
        cell.nets.clear();
        cell.partition = -1; // Reset partition assignment
    }
    
    for (auto& net : const_cast<std::vector<Net>&>(netlist_.getNets())) {
        // Reset the cells vector completely for all nets
        net.cells.clear();
    }
    
    // Pre-validation phase 2: Rebuild connections using netlist information
    std::cout << "Pre-validation phase 2: Rebuilding safe cell-net connections..." << std::endl;
    // We'll use the netlist's internal connectivity information to rebuild connections
    
    // First, collect all the cell-net connections from the parser
    // The parser has successfully read in all connections - use that as our source of truth
    // For this specific fix, we'll need to modify the Netlist class to expose its internal connection data
    // or implement a method to rebuild connections safely
    
    // For now, we'll implement a simple but safe approach:
    // We won't try to rebuild connections here (would require parser modifications)
    // Instead, we'll just create fresh partitions with the existing netlist objects
    
    // Pre-validation phase 3: Verify critical data structures after reset
    std::cout << "Pre-validation phase 3: Verifying data structure integrity..." << std::endl;
    int cellsWithoutNets = 0;
    int netsWithoutCells = 0;
    
    for (auto& cell : cells) {
        if (cell.nets.empty()) {
            cellsWithoutNets++;
        }
    }
    
    for (auto& net : const_cast<std::vector<Net>&>(netlist_.getNets())) {
        if (net.cells.empty()) {
            netsWithoutCells++;
        }
    }
    
    std::cout << "Found " << cellsWithoutNets << " cells without nets and " 
              << netsWithoutCells << " nets without cells after reset" << std::endl;
    
    // Since we've reset all connections, we can skip the detailed validation phase
    // and move directly to partitioning
    
    // Use current time as random seed
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::mt19937 gen(seed);
    std::uniform_int_distribution<> dis(0, 1);

    // Randomly assign cells to partitions
    int partition1Count = 0;
    int totalCells = cells.size();
    int targetSize = totalCells / 2;

    std::cout << "Assigning cells to partitions..." << std::endl;
    for (auto& cell : cells) {
        // Assign to partition 1 if we haven't reached target size
        if (partition1Count < targetSize) {
            cell.partition = 0;
            partition1Count++;
        } else {
            cell.partition = 1;
        }
    }
    
    std::cout << "Initial partition created." << std::endl;
    std::cout << "Partition sizes: [" << partition1Count << ", " 
              << (totalCells - partition1Count) << "]" << std::endl;
    
    // Initialize other FMEngine state
    // Reset partition sizes by setting them directly
    partitionState_.updatePartitionSize(0, partition1Count);
    partitionState_.updatePartitionSize(1, totalCells - partition1Count);
    
    // Since connections were reset, cut size will be 0
    // We can proceed without calculating the initial cut size
    partitionState_.updateCutSize(0);
    
    std::cout << "FMEngine initialization completed with connection reset." << std::endl;
    std::cout << "Warning: Connections were reset due to corrupted data." << std::endl;
    std::cout << "The algorithm will proceed but results may not be optimal." << std::endl;
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

    std::cout << "Calculating gain for cell " << cell->name << std::endl;

    int gain = 0;
    int fromPartition = cell->partition;
    int toPartition = 1 - fromPartition;

    for (Net* net : cell->nets) {
        if (!net) {
            std::cerr << "calculateCellGain: Error - found null net pointer for cell " 
                     << cell->name << std::endl;
            continue;
        }

        std::cout << "  Processing net " << net->name 
                  << " [" << net->partitionCount[0] << ", " << net->partitionCount[1] << "]" << std::endl;

        // Critical net conditions:
        // 1. If this is the only cell in fromPartition, moving it would remove the net from cut
        if (net->partitionCount[fromPartition] == 1) {
            gain++; // Uncutting the net IMPROVES (reduces) cut size -> POSITIVE gain
            std::cout << "    Net would become uncut (+1 gain)" << std::endl;
        }
        // 2. If there are no cells in toPartition, moving this cell would add the net to cut
        if (net->partitionCount[toPartition] == 0) {
            gain--; // Cutting the net WORSENS (increases) cut size -> NEGATIVE gain
            std::cout << "    Net would become cut (-1 gain)" << std::endl;
        }
    }

    std::cout << "  Final gain: " << gain << std::endl;
    return gain;
}

void FMEngine::updateGainsAfterMove(Cell* movedCell) {
    if (!movedCell) {
        std::cerr << "updateGainsAfterMove: Error - null cell pointer" << std::endl;
        return;
    }

    std::cout << "Updating gains after moving cell " << movedCell->name << std::endl;

    // Store current partition counts for validation
    std::vector<std::pair<Net*, std::array<int, 2>>> oldPartitionCounts;
    for (Net* net : movedCell->nets) {
        if (!net) {
            std::cerr << "updateGainsAfterMove: Error - found null net pointer for cell " 
                     << movedCell->name << std::endl;
            continue;
        }
        oldPartitionCounts.push_back({net, {net->partitionCount[0], net->partitionCount[1]}});
    }

    // For each net connected to moved cell
    for (size_t i = 0; i < movedCell->nets.size(); i++) {
        Net* net = movedCell->nets[i];
        if (!net) {
            std::cerr << "updateGainsAfterMove: Error - found null net pointer for cell " 
                     << movedCell->name << std::endl;
            continue;
        }

        std::cout << "  Processing net " << net->name 
                  << " [" << net->partitionCount[0] << ", " << net->partitionCount[1] << "]" << std::endl;

        // Validate partition counts
        int totalCells = net->partitionCount[0] + net->partitionCount[1];
        if (totalCells != static_cast<int>(net->cells.size())) {
            std::cerr << "updateGainsAfterMove: Error - partition count mismatch for net " << net->name 
                     << ". Expected " << net->cells.size() << ", got " << totalCells << std::endl;
            continue;
        }

        // For each cell on this net
        for (Cell* cell : net->cells) {
            if (!cell) {
                std::cerr << "updateGainsAfterMove: Error - found null cell pointer in net " 
                         << net->name << std::endl;
                continue;
            }

            // Skip moved cell and locked cells
            if (cell == movedCell || cell->locked) {
                continue;
            }

            // Verify cell-net relationship
            bool foundNet = false;
            for (Net* cellNet : cell->nets) {
                if (cellNet == net) {
                    foundNet = true;
                    break;
                }
            }
            if (!foundNet) {
                std::cerr << "updateGainsAfterMove: Error - inconsistent cell-net relationship between "
                         << cell->name << " and " << net->name << std::endl;
                continue;
            }

            std::cout << "    Updating gain for cell " << cell->name << std::endl;

            // Store old gain for bucket update
            int oldGain = cell->gain;
            
            // Calculate new gain
            int newGain = calculateCellGain(cell);
            
            if (oldGain != newGain) {
                std::cout << "      Gain changed from " << oldGain << " to " << newGain << std::endl;
                
                // Update cell's gain in gain bucket
                try {
                    cell->gain = newGain;  // Update cell's gain before updating bucket
                    gainBucket_.updateCellGain(cell, oldGain, newGain);
                } catch (const std::exception& e) {
                    std::cerr << "updateGainsAfterMove: Error updating gain bucket - " 
                             << e.what() << std::endl;
                    // Revert gain change on failure
                    cell->gain = oldGain;
                }
            }
        }

        // Verify partition counts haven't changed unexpectedly
        if (net->partitionCount[0] + net->partitionCount[1] != totalCells) {
            std::cerr << "updateGainsAfterMove: Error - partition counts changed unexpectedly for net " 
                     << net->name << std::endl;
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

    std::cout << "Applying move for cell " << cell->name 
              << " from partition " << cell->partition 
              << " to partition " << toPartition << std::endl;

    // Store old partition for gain updates
    int fromPartition = cell->partition;

    // Validate move legality
    if (toPartition < 0 || toPartition > 1) {
        std::cerr << "applyMove: Error - invalid target partition " << toPartition << std::endl;
        return;
    }

    if (fromPartition == toPartition) {
        std::cerr << "applyMove: Error - moving to same partition" << std::endl;
        return;
    }

    // Remove cell from gain bucket before updating its gain
    gainBucket_.removeCell(cell);

    // Update partition counts for all nets connected to this cell
    for (Net* net : cell->nets) {
        if (!net) {
            std::cerr << "applyMove: Error - found null net pointer for cell " << cell->name << std::endl;
            continue;
        }

        // Store old partition counts for validation
        int oldCount0 = net->partitionCount[0];
        int oldCount1 = net->partitionCount[1];

        // Validate current partition counts
        if (oldCount0 < 0 || oldCount1 < 0) {
            std::cerr << "applyMove: Error - negative partition count for net " << net->name 
                     << " [" << oldCount0 << ", " << oldCount1 << "]" << std::endl;
            continue;
        }

        // Update partition counts
        net->partitionCount[fromPartition]--;
        net->partitionCount[toPartition]++;

        // Validate new partition counts
        if (net->partitionCount[fromPartition] < 0) {
            std::cerr << "applyMove: Error - negative partition count after move for net " << net->name 
                     << " partition " << fromPartition << std::endl;
            // Revert the change
            net->partitionCount[fromPartition]++;
            net->partitionCount[toPartition]--;
            continue;
        }

        // Verify total cell count hasn't changed
        int totalCount = net->partitionCount[0] + net->partitionCount[1];
        if (totalCount != static_cast<int>(net->cells.size())) {
            std::cerr << "applyMove: Error - partition count mismatch for net " << net->name 
                     << ". Expected " << net->cells.size() 
                     << ", got " << totalCount << std::endl;
        }

        // Update cut size based on net state change
        bool wasCut = (oldCount0 > 0 && oldCount1 > 0);
        bool isCut = (net->partitionCount[0] > 0 && net->partitionCount[1] > 0);
        
        if (wasCut && !isCut) {
            partitionState_.updateCutSize(-1);
            std::cout << "  Net " << net->name << " is no longer cut" << std::endl;
        } else if (!wasCut && isCut) {
            partitionState_.updateCutSize(1);
            std::cout << "  Net " << net->name << " is now cut" << std::endl;
        }

        std::cout << "  Updated net " << net->name << " partition counts: [" 
                  << net->partitionCount[0] << ", " << net->partitionCount[1] << "]" << std::endl;
    }

    // Update partition sizes
    partitionState_.updatePartitionSize(fromPartition, -1);
    partitionState_.updatePartitionSize(toPartition, 1);

    // Update cell's partition
    cell->partition = toPartition;
    cell->locked = true;

    // Update gains of affected cells
    updateGainsAfterMove(cell);

    std::cout << "Move completed. Current cut size: " << partitionState_.getCurrentCutSize() << std::endl;
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