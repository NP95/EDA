# Fiduccia-Mattheyses (F-M) Partitioning Implementation Design Document

## 1. Introduction

### 1.1. Purpose
This document outlines the design and implementation plan for a 2-way circuit partitioner using the Fiduccia-Mattheyses (F-M) algorithm, as specified in the ECE5960 Programming Assignment #1 [cite: User Prompt: F-M Circuit Partitioning Assignment]. The goal is to partition a set of cells connected by nets into two balanced groups while minimizing the number of nets cut.

### 1.2. Problem Overview
Given a circuit hypergraph consisting of cells (C) and nets (N), and a balance factor (r), the task is to divide C into two disjoint partitions, G<sub>1</sub> and G<sub>2</sub>, such that:

- The number of nets connecting cells in both G<sub>1</sub> and G<sub>2</sub> (the cut size) is minimized.
- The size of each partition satisfies the balance constraint: n×(1−r)/2≤∣G<sub>1</sub>∣,∣G<sub>2</sub>∣≤n×(1+r)/2, where n=∣C∣. 

The Fiduccia-Mattheyses heuristic will be implemented to iteratively improve an initial partition.

### 1.3. Key Requirements [cite: User Prompt: F-M Circuit Partitioning Assignment]

- Read input files specifying the balance factor (r) and netlist details (NET NET_NAME [CELL_NAME]+ ;).
- Implement the core F-M algorithm, including gain calculation, bucket list management, cell move selection, and gain updates. 
- Strictly enforce the balance constraint on the final partition sizes.
- Output the final cut size, the size of G<sub>1</sub> and G<sub>2</sub>, and the cell names within each group in the specified format.
- The program must accept command-line arguments: [executable] [input_file] [output_file].
- The implementation must pass verification using the provided checker_linux program.
- Aim for good solution quality (low cut size) and reasonable runtime performance.

### 1.4. Design Goals

- **Correctness**: Implement the F-M algorithm accurately and produce results that pass the checker and satisfy balance constraints.
- **Solution Quality**: Achieve low cut sizes compared to initial or naive partitions.
- **Efficiency**: Implement data structures and algorithms efficiently for competitive runtime, particularly the gain bucket and update mechanisms.
- **Modularity**: Separate parsing, data structure management, and the core F-M algorithm logic.
- **Readability**: Write clear, well-commented code (primarily for grading and potential future maintenance).
- **Testability**: Allow for easy testing using the provided checker and potentially smaller, custom test cases.

## 2. System Architecture

### 2.1. Overall Structure
The system will be organized as follows:

```
FM_Partitioner/
├── DataStructures/
│   ├── Netlist.{h,cpp}       # Represents Cells and Nets 
│   ├── PartitionState.{h,cpp} # Tracks G1/G2 sizes, cutsize
│   └── GainBucket.{h,cpp}    # Manages free cells by gain
├── IO/  
│   ├── Parser.{h,cpp}        # Reads input file format
│   └── OutputGenerator.{h,cpp} # Writes results to output file
├── Algorithm/
│   └── FMEngine.{h,cpp}      # Implements the core F-M logic
└── Main.cpp                  # Orchestrates parsing, execution, output
```

### 2.2. Data Flow

1. **Input Processing** (Parser): Read the balance factor r and the netlist from the input file. Build the Netlist data structure (cells and nets with connectivity).

2. **Initialization** (FMEngine): Create an initial partition (e.g., random assignment respecting initial balance loosely). Calculate initial cell gains and populate the GainBucket. Initialize PartitionState. 

3. **F-M Iteration** (FMEngine): 
   - Execute F-M passes repeatedly.
   - Each pass involves selecting and moving the highest-gain unlocked cell that maintains intermediate balance, updating neighbour gains, and locking the moved cell.
   - Track the best partition state (minimum cut size) seen during the pass. 
   - After the pass, revert moves to achieve the best state found in that pass.
   - Repeat passes until no improvement in cut size is observed.

4. **Output Generation** (OutputGenerator): Using the final PartitionState and Netlist (cell assignments), format and write the cut size, group sizes, and cell lists to the output file.

### 2.3. Component Interactions

- Main calls Parser to populate Netlist.
- Main initializes FMEngine with Netlist and r.  
- FMEngine uses and modifies Netlist (cell partitions), PartitionState, and GainBucket.
- Main calls OutputGenerator with the final state from FMEngine and Netlist.

## 3. Core Data Structures

### 3.1. Netlist Representation (Netlist.{h,cpp})

**Cell Structure:**
```cpp
struct Net; // Forward declaration
struct BucketNode; // Forward declaration for linking to GainBucket

struct Cell {
    std::string name;
    int id;                     // Unique integer ID
    int partition = 0;          // e.g., 0 for G1, 1 for G2
    int gain = 0;
    bool locked = false;
    std::vector<int> netIds;    // IDs of connected nets (NEW - resolved memory issues)
    BucketNode* bucketNodePtr = nullptr; // Pointer to node in GainBucket's linked list
    // Potentially add temporary gain storage if needed during updates
};
```

**Net Structure:**
```cpp
struct Cell; // Forward declaration

struct Net {
    std::string name;
    int id;                     // Unique integer ID
    std::vector<int> cellIds;   // IDs of connected cells (NEW - resolved memory issues)
    int partitionCount[2] = {0, 0}; // Count of cells in G1 (index 0) and G2 (index 1)
};
```

**Netlist Class:**
- `std::vector<Cell> cells_`: Stores all cells, indexed by id.
- `std::vector<Net> nets_`: Stores all nets, indexed by id.
- `std::unordered_map<std::string, int> cellNameToId_`: Map name to internal ID.
- `std::unordered_map<std::string, int> netNameToId_`: Map name to internal ID.
- Methods to add cells, nets, connections, get cells/nets by name or ID.

### 3.2. Partition State (PartitionState.{h,cpp})

- `int partitionSize[2]`: Current number of cells in G1 and G2.
- `int currentCutsize`: Current cut size.
- `double balanceFactor`: The required r.
- `int totalCells`: Total number of cells n.
- `int minPartitionSize, maxPartitionSize`: Pre-calculated balance limits.
- Methods to check balance, update cut size, update partition sizes.

### 3.3. Gain Bucket (GainBucket.{h,cpp})

**Bucket Node Structure:** (Nodes of the doubly-linked lists)
```cpp
struct BucketNode {
    Cell* cellPtr = nullptr;    // Pointer back to the cell
    BucketNode *prev = nullptr, *next = nullptr;
    // Gain value is implicitly known from the list it's in
};
```

**GainBucket Class:**
- `std::vector<BucketNode*> buckets_[2]`: Array of lists (one for G1, one for G2). Each element `buckets_[p][gain_index]` is the head of a doubly-linked list for partition p and a specific gain. Gain needs to be mapped to an index (e.g., gain + max_degree).
- `int maxGain_[2]`: Tracks the highest gain currently present in each partition's bucket.
- `int maxPossibleDegree`: Maximum number of nets connected to any single cell (determines range of gains).
- Methods:
  - `initialize(cells, max_degree)`: Calculate initial gains and build lists.
  - `addCell(Cell* cell)`: Add a cell to the correct list based on its gain and partition.
  - `removeCell(Cell* cell)`: Remove a cell's node from its current list.
  - `updateCellGain(Cell* cell, int oldGain, int newGain)`: Efficiently move a cell's node between lists.
  - `getBestFeasibleCell()`: Find and return the highest-gain unlocked cell whose move would not violate balance constraints. Needs careful implementation regarding intermediate vs final balance.
  - `getMaxGain(int partition)`: Return the current max gain for a partition.

## 4. Core Algorithms (FMEngine.{h,cpp})

### 4.1. Initialization (FMEngine::initializePartitions)

1. Assign cells randomly or sequentially to G1/G2, aiming for initial balance.
2. Calculate `net.partitionCount` for all nets based on the initial assignment.
3. Calculate initial `cell.gain` for all cells (Gain = #nets that become uncut if moved - #nets that become cut if moved).
4. Initialize `GainBucket` by adding all cells.
5. Calculate initial `PartitionState.currentCutsize`.
6. Determine `minPartitionSize` and `maxPartitionSize` from n and r.

### 4.2. Single F-M Pass (FMEngine::runPass)

1. Initialize `bestCutsizeThisPass` to current cutsize, clear move log/stack.
2. Mark all cells as unlocked. Reset `GainBucket::maxGain_`. (Gain values carry over, bucket structure reflects current gains).
3. Loop n times (or until no valid moves):
   - Find `baseCell = GainBucket::getBestFeasibleCell()`. This involves checking `maxGain_` downwards in both G1 and G2 buckets and selecting the highest gain cell whose move maintains balance (e.g., keeps partition sizes within [minSize-1, maxSize+1] or similar relaxed bound during the pass). If no such cell exists, break the loop.
   - If `baseCell` is found:
     - Record the move (cell, from_partition, to_partition, gain). 
     - `GainBucket::removeCell(baseCell)`.
     - `baseCell->locked = true`.
     - Update `PartitionState` (sizes, `currentCutsize += baseCell->gain`). Note: The gain calculation needs care here. It's often simpler to recalculate cutsize change based on net states. Re-calculate cutsize change: For each net on `baseCell`, check if it was cut and becomes uncut (-1), or was uncut and becomes cut (+1). Sum these changes. Update `PartitionState.currentCutsize`.
     - Move the cell: `baseCell->partition = other_partition`. Update `net.partitionCount` for all nets connected to `baseCell`.
     - **Update Neighbor Gains**:
       - For each net connected to `baseCell`:
         - Check `net.partitionCount` before and after the move. 
         - Determine if the net is critical (changes from cut/uncut or distribution changes critically impacting neighbor gains).
         - For each `neighborCell` connected to this net (if `!neighborCell->locked`):
           - Recalculate `neighborCell`'s gain based on the new state of its connected nets.
           - `GainBucket::updateCellGain(neighborCell, oldGain, newGain)`.
     - Store the `currentCutsize` and the number of moves made so far.
4. After the loop, find the point (number of moves) in the recorded sequence that yielded the minimum `currentCutsize` (`bestMoveIndex`).
5. Revert moves from the end back to the state after `bestMoveIndex`.
   - **Important Refinement:** Instead of trusting the `currentCutsize` recorded in the move history at `bestMoveIndex`, recalculate the cut size from scratch based on the final partition state after reverting using a helper like `calculateCurrentCutSize()` to ensure correctness, especially for edge cases encountered during debugging.
6. If `bestMoveIndex` is -1 (no improvement found during the pass), revert all moves. Reset cut size to the value at the start of the pass.
7. Return true if `bestMoveIndex >= 0` and the best cut size found was strictly less than the initial cut size of the pass, otherwise return false.

### 4.3. Main Loop (FMEngine::run)

1. Call `initializePartitions`.
2. Loop:
   - Call `runPass`.
   - If `runPass` returns false, break the loop.
3. The final state in `Netlist` and `PartitionState` represents the result.

## 5. Testing Strategy

### 5.1. Unit Testing
Test `GainBucket` operations (add, remove, update, find max), gain calculation logic for individual cells, net partition counting.

### 5.2. Integration Testing
Use small, hand-craftable netlists to verify the parser and the full `runPass` logic step-by-step.

### 5.3. Validation
Use the provided `checker_linux` on all assignment benchmarks [cite: User Prompt: F-M Circuit Partitioning Assignment].
```bash
./checker_linux [input_file] [output_file]
```

### 5.4. Benchmarking
Record cut size and runtime for each benchmark where the checker passes, for inclusion in the report. Use `time` command or internal timers.

## 6. Implementation Details

### 6.1. Parser (Parser.{h,cpp})

- Use standard C++ streams (`ifstream`).
- Read r first.
- Loop reading lines or tokens. Identify NET, net name, cell names, and `;`. 
- Use `std::unordered_map` to efficiently map string names (cells, nets) to integer IDs and store them in `Netlist` vectors. Handle new cells/nets encountered.

### 6.2. Gain Bucket Implementation (GainBucket.{h,cpp})

- Use `std::vector<BucketNode*>` where the index maps to gain (offset by `maxPossibleDegree`). Each `BucketNode*` is the head of a doubly-linked list.
- Implement list operations (insert head, remove node) carefully, updating `prev/next` pointers and the head pointer in the vector.
- Store `BucketNode*` in the `Cell` struct for O(1) access to the node during removal/updates.

### 6.3. Gain Update Logic (FMEngine::runPass)

This is performance-critical. When a cell moves, iterate its nets.

For each net, check `partitionCount` before/after move.
- If `net.partitionCount[to_partition]` becomes 1 (net becomes cut) -> Increment gain of unlocked neighbors in `from_partition`.
- If `net.partitionCount[from_partition]` becomes 0 (net becomes uncut) -> Decrement gain of unlocked neighbors in `to_partition`.
- If `net.partitionCount[from_partition]` was 1 (net was cut) -> Decrement gain of unlocked neighbors in `from_partition`.
- If `net.partitionCount[to_partition]` was 0 (net was uncut) -> Increment gain of unlocked neighbors in `to_partition`.

Update the affected neighbor's gain value and call `GainBucket::updateCellGain`.

### 6.4. Output Generation (OutputGenerator.{h,cpp})

1. Open the output file using `ofstream`. 
2. Write `Cutsize = [value]`.
3. Collect cells belonging to G1 and G2 from the final `Netlist` state.
4. Write `G1 [size]` followed by cell names, ending with `;`.
5. Write `G2 [size]` followed by cell names, ending with `;`.

## 7. Performance Considerations

### 7.1. Data Structures
Efficient Gain Bucket (O(1) operations ideally). `unordered_map` for name lookups. `vector` for indexed access to cells/nets. Pointers for direct access between cells/nets/bucket nodes.

### 7.2. Algorithm
The gain update logic within `runPass` is the bottleneck. Minimize redundant calculations. Efficiently find the best feasible cell.

### 7.3. Memory
Avoid excessive copying. Use pointers and references where appropriate. Pre-allocate vectors if sizes are known (`cells_`, `nets_`). 

### 7.4. Compiler Optimization
Compile with `-O2` or `-O3` (achieved via `CMAKE_BUILD_TYPE=Release`).

### 7.5. Language Choice
C++ is recommended for performance.

### 7.6. Adaptive Early Pass Termination (Added)
To significantly reduce runtime on large benchmarks, passes are terminated early if no improvement in the best-found cut size occurs for a certain number of moves. This threshold is adaptive, starting higher in early passes (e.g., 2000) and decreasing in later passes (down to a minimum, e.g., 500) to balance exploration and efficiency.

## 8. Phased Implementation Plan

Based on the assignment checkpoint structure, the implementation will be divided into two main phases:

### 8.1. Phase 1 (First Checkpoint)

**Focus: Foundation and Basic Data Structures**

1. **Parser Implementation**
   - Implement input file parsing to extract balance factor and netlist information
   - Create basic data structures to represent cells and nets
   - Set up connectivity information between cells and nets

2. **Basic Netlist Structure**
   - Implement `