#include "PartitionState.h"
#include <cmath>
#include <stdexcept>

namespace fm {

PartitionState::PartitionState(int totalCells, double balanceFactor)
    : balanceFactor(balanceFactor)
    , totalCells(totalCells) {
    calculateBalanceLimits();
}

bool PartitionState::isBalanced(int partition1Size, int partition2Size) const {
    return partition1Size >= minPartitionSize && 
           partition1Size <= maxPartitionSize &&
           partition2Size >= minPartitionSize && 
           partition2Size <= maxPartitionSize;
}

void PartitionState::calculateBalanceLimits() {
    // Calculate minimum and maximum partition sizes based on balance factor
    // For n cells and balance factor r:
    // n*(1-r)/2 ≤ |G1|,|G2| ≤ n*(1+r)/2
    
    double halfSize = totalCells / 2.0;
    minPartitionSize = static_cast<int>(std::ceil(halfSize * (1.0 - balanceFactor)));
    maxPartitionSize = static_cast<int>(std::floor(halfSize * (1.0 + balanceFactor)));
}

void PartitionState::setCurrentCutSize(int cutSize) {
    currentCutsize_ = cutSize;
}

} // namespace fm 