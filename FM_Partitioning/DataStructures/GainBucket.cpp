#include "GainBucket.h"
#include "PartitionState.h"
#include <stdexcept>

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
    // Add all cells to their respective gain buckets
    for (const auto& cell : cells) {
        if (!cell.locked) {
            addCell(const_cast<Cell*>(&cell));  // Safe as cells come from Netlist
        }
    }
}

void GainBucket::addCell(Cell* cell) {
    if (!cell) return;

    // Create new node
    BucketNode* node = createNode(cell);
    cell->bucketNodePtr = node;

    // Get appropriate bucket
    int index = gainToIndex(cell->gain);
    int partition = cell->partition;

    // Insert at head of list
    node->next = buckets_[partition][index];
    if (node->next) {
        node->next->prev = node;
    }
    buckets_[partition][index] = node;

    // Update max gain if necessary
    if (cell->gain > maxGain_[partition]) {
        maxGain_[partition] = cell->gain;
    }
}

void GainBucket::removeCell(Cell* cell) {
    if (!cell || !cell->bucketNodePtr) return;

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

void GainBucket::updateCellGain(Cell* cell, int /*oldGain*/, int newGain) {
    // Remove from old bucket and add to new bucket
    removeCell(cell);
    cell->gain = newGain;
    addCell(cell);
}

Cell* GainBucket::getBestFeasibleCell(const PartitionState& state) {
    // Try both partitions
    for (int p = 0; p < 2; p++) {
        // Start from maximum gain and work down
        for (int gain = maxGain_[p]; gain >= -maxPossibleDegree; gain--) {
            int index = gainToIndex(gain);
            BucketNode* node = buckets_[p][index];
            
            while (node) {
                Cell* cell = node->cellPtr;
                if (!cell->locked) {
                    // Check if moving this cell maintains balance
                    int otherPartition = 1 - p;
                    int newSize1 = state.getPartitionSize(p) - 1;
                    int newSize2 = state.getPartitionSize(otherPartition) + 1;
                    
                    if (state.isBalanced(newSize1, newSize2)) {
                        return cell;
                    }
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