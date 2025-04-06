# F-M Circuit Partitioning

Implementing Professor T.W Huang's assignment from his course in Utah.

# Fiduccia-Mattheyses (F-M) Circuit Partitioning Implementation

## Overview

This project implements the Fiduccia-Mattheyses (F-M) algorithm for 2-way circuit partitioning. The F-M algorithm is a widely used heuristic in Electronic Design Automation (EDA) for partitioning circuits into balanced groups while minimizing the interconnections between them.

## Problem Description

Given:
- A set of cells C = {c₁, c₂, c₃, ..., cₙ}
- A set of nets N = {n₁, n₂, n₃, ..., nₘ} (where each net connects a subset of cells)
- A balance factor r (0 < r < 1)

The goal is to:
- Partition C into two disjoint, balanced groups G₁ and G₂
- Minimize the cut size (number of nets crossing between G₁ and G₂)
- Satisfy the balance constraint: n×(1-r)/2 ≤ |G₁|,|G₂| ≤ n×(1+r)/2
- No cell replication is allowed

## Key Features

### Core Algorithm Implementation
- Complete F-M algorithm with proper gain calculation
- Bucket data structure for efficient highest-gain cell selection
- Multiple passes with best-state tracking and rollback
- Initial partition generation with balance constraints

### Performance Optimizations
1. **Selective Gain Updates** - Instead of recalculating gains for all neighbors after each move, implements an incremental approach that only updates cells affected by critical state transitions, resulting in dramatic performance improvements:
   - Up to 45x speedup on large benchmarks
   - Careful tracking of partition counts before/after cell moves

2. **Adaptive Early Pass Termination** - Dynamically adjusts the threshold for early termination based on pass number:
   - Starts with a higher threshold (2000 moves without improvement)
   - Gradually decreases to a minimum (500 moves)
   - Balances solution quality with runtime performance

3. **Optimized Data Structures**:
   - Cell-net relationships stored as ID references instead of pointers
   - Efficient gain bucket implementation using doubly-linked lists
   - Careful memory management to avoid redundant allocation

### Additional Features
- Robust input parsing with error handling
- Modular design separating algorithm, data structures, and I/O
- Cut size verification and recalculation for reliability
- Comprehensive logging for debugging and analysis

## Performance Results

Performance improvements from optimizations (with `-O3` compiler optimization):

| Benchmark    | Baseline Runtime | Optimized Runtime | Speedup |
|--------------|------------------|-------------------|---------|
| input_0.dat  | ~11 minutes      | ~15 seconds       | ~45x    |
| input_5.dat  | ~82 minutes      | ~10 seconds       | ~492x   |

All benchmarks pass the provided checker program while achieving significant performance improvements.

## Project Structure

```
FM_Partitioner/
├── DataStructures/           # Core data structures
│   ├── Netlist.{h,cpp}       # Cells and nets representation
│   ├── PartitionState.{h,cpp}# Tracks partition balance and cut size
│   └── GainBucket.{h,cpp}    # Bucket list for cell selection
├── IO/                       # Input/output handling
│   ├── Parser.{h,cpp}        # Input file parsing
│   └── OutputGenerator.{h,cpp}# Results output generation
├── Algorithm/
│   └── FMEngine.{h,cpp}      # Core F-M algorithm implementation
├── CMakeLists.txt            # Build configuration with -O3 optimization
├── main.cpp                  # Program entry point
└── README.md                 # This file
```

## Build and Usage

### Prerequisites
- C++17 compatible compiler
- CMake 3.10+

### Building
```bash
mkdir build
cd build
cmake ..
make
```

### Running
```bash
./fm [input_file] [output_file]
```

Example:
```bash
./fm input_pa1/input_0.dat output_0.dat
```

### Verifying Results
```bash
./checker_linux [input_file] [output_file]
```

## Algorithm Details

The F-M algorithm implementation uses the following key approaches:

1. **Initial Partition**: Cells are initially assigned to achieve a balanced partition that satisfies the balance factor constraint.

2. **Gain Calculation**: For each cell, gain represents the reduction in cut size if the cell is moved to the opposite partition.

3. **Pass Structure**: Each pass:
   - Selects highest-gain unlocked cell that maintains balance
   - Moves cell to opposite partition and locks it
   - Updates gains of affected neighbors using selective gain updates
   - Tracks best cut size achieved during the pass
   - After all moves, reverts to the state with minimum cut size

4. **Termination**: Algorithm terminates when a pass produces no improvement or after a maximum number of passes.

## Technical Highlights

The most significant optimization is the selective gain update strategy which follows these steps:

1. For each net connected to a moved cell, track partition counts before and after move
2. For each unlocked neighbor cell connected through the net:
   - Apply F-M gain update rules based on critical state transitions
   - Calculate gain delta and update incrementally instead of full recalculation
   - Update cell position in gain bucket only when necessary

This selective approach dramatically reduces computational overhead in the gain update process, which was identified as the primary performance bottleneck.
