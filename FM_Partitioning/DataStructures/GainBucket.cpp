#include "GainBucket.h"
#include "PartitionState.h"
#include <stdexcept>
#include <iostream>

namespace fm {

GainBucket::GainBucket(int maxPossibleDegree) 
    : maxPossibleDegree(maxPossibleDegree) {
    // Initialize bucket lists for both partitions
    // Size is 2*maxPossibleDegree + 1 to accommodate gains from -maxDegree to +maxDegree
    int bucketSize = 2 * maxPossibleDegree + 1;
    buckets_[0].resize(bucketSize, nullptr);
    buckets_[1].resize(bucketSize, nullptr);
}

GainBucket::~GainBucket() {
    // Clean up all nodes in both partitions
    for (int p = 0; p < 2; p++) {
        for (auto& head : buckets_[p]) {
            while (head) {
                BucketNode* next = head->next;
                delete head;
                head = next;
            }
        }
    }
}

void GainBucket::initialize(const std::vector<Cell>& cells) {
    std::cout << "Initializing gain buckets..." << std::endl;
    // Clear existing buckets
    for (int p = 0; p < 2; p++) {
        for (auto& head : buckets_[p]) {
            while (head) {
                BucketNode* next = head->next;
                delete head;
                head = next;
            }
            head = nullptr;
        }
        maxGain_[p] = -maxPossibleDegree;
    }

    // Add all unlocked cells to their respective gain buckets
    for (const auto& cell : cells) {
        if (!cell.locked) {
            addCell(const_cast<Cell*>(&cell));
        }
    }

    std::cout << "Gain buckets initialized. Max gains: ["
              << maxGain_[0] << ", " << maxGain_[1] << "]" << std::endl;
}

void GainBucket::addCell(Cell* cell) {
    if (!cell) {
        std::cerr << "addCell: Error - null cell pointer" << std::endl;
        return;
    }

    if (cell->bucketNodePtr) {
        std::cerr << "addCell: Warning - cell " << cell->name 
                  << " already has a bucket node" << std::endl;
        removeCell(cell);
    }

    // Create new node
    BucketNode* node = createNode(cell);
    cell->bucketNodePtr = node;

    // Get appropriate bucket
    int index = gainToIndex(cell->gain);
    int partition = cell->partition;

    if (index < 0 || index >= static_cast<int>(buckets_[partition].size())) {
        std::cerr << "addCell: Error - invalid gain index " << index 
                  << " for cell " << cell->name << std::endl;
        delete node;
        cell->bucketNodePtr = nullptr;
        return;
    }

    // Insert at head of list
    node->next = buckets_[partition][index];
    if (node->next) {
        node->next->prev = node;
    }
    buckets_[partition][index] = node;

    // Update max gain if necessary
    if (cell->gain > maxGain_[partition]) {
        maxGain_[partition] = cell->gain;
        std::cout << "  Updated max gain for partition " << partition 
                  << " to " << maxGain_[partition] << std::endl;
    }
}

void GainBucket::removeCell(Cell* cell) {
    if (!cell || !cell->bucketNodePtr) {
        return;
    }

    BucketNode* node = cell->bucketNodePtr;
    int index = gainToIndex(cell->gain);
    int partition = cell->partition;

    // Update list pointers
    if (node->prev) {
        node->prev->next = node->next;
    } else {
        buckets_[partition][index] = node->next;
    }
    
    if (node->next) {
        node->next->prev = node->prev;
    }

    // Clean up
    delete node;
    cell->bucketNodePtr = nullptr;

    // Update max gain if necessary
    if (cell->gain == maxGain_[partition]) {
        updateMaxGain(partition);
    }
}

void GainBucket::updateCellGain(Cell* cell, int oldGain, int newGain) {
    if (!cell) {
        std::cerr << "updateCellGain: Error - null cell pointer" << std::endl;
        return;
    }

    std::cout << "Updating gain for cell " << cell->name 
              << " from " << oldGain << " to " << newGain << std::endl;

    // Remove from old bucket
    removeCell(cell);

    // Update cell's gain
    cell->gain = newGain;

    // Add to new bucket
    addCell(cell);

    // Update max gain if necessary
    if (newGain > maxGain_[cell->partition]) {
        maxGain_[cell->partition] = newGain;
        std::cout << "  Updated max gain for partition " << cell->partition 
                  << " to " << maxGain_[cell->partition] << std::endl;
    }
}

Cell* GainBucket::getBestFeasibleCell(const PartitionState& state) {
    // Try both partitions
    for (int p = 0; p < 2; p++) {
        // Start from maximum gain and work down
        for (int gain = maxGain_[p]; gain >= -maxPossibleDegree; gain--) {
            int index = gainToIndex(gain);
            if (index < 0 || index >= static_cast<int>(buckets_[p].size())) {
                continue;
            }

            BucketNode* node = buckets_[p][index];
            while (node) {
                Cell* cell = node->cellPtr;
                if (!cell) {
                    std::cerr << "getBestFeasibleCell: Error - found null cell pointer" << std::endl;
                    node = node->next;
                    continue;
                }

                if (cell->locked) {
                    node = node->next;
                    continue;
                }

                // Check if moving this cell maintains balance
                int otherPartition = 1 - p;
                int newSize1 = state.getPartitionSize(p) - 1;
                int newSize2 = state.getPartitionSize(otherPartition) + 1;
                
                if (state.isBalanced(newSize1, newSize2)) {
                    std::cout << "Found feasible cell " << cell->name 
                              << " with gain " << cell->gain << std::endl;
                    return cell;
                }
                node = node->next;
            }
        }
    }
    return nullptr;
}

int GainBucket::gainToIndex(int gain) const {
    return gain + maxPossibleDegree;  // Shift to make all indices non-negative
}

void GainBucket::updateMaxGain(int partition) {
    maxGain_[partition] = -maxPossibleDegree;
    
    // Scan buckets from high to low to find new max gain
    for (int gain = maxPossibleDegree; gain >= -maxPossibleDegree; gain--) {
        int index = gainToIndex(gain);
        if (buckets_[partition][index] != nullptr) {
            maxGain_[partition] = gain;
            break;
        }
    }
}

BucketNode* GainBucket::createNode(Cell* cell) {
    BucketNode* node = new BucketNode();
    node->cellPtr = cell;
    node->prev = nullptr;
    node->next = nullptr;
    return node;
}

} // namespace fm 