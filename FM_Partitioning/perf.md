# Performance Optimization Log for FM Algorithm

This document tracks the performance optimization efforts applied to the C++ F-M partitioner.

## Baseline (Before Optimizations)

*   Initial functionally correct version (passed benchmarks 0-4, 6 in Debug w/ ASan).
*   Release build (`-O3`) established baseline performance:
    *   `input_0.dat`: ~11 minutes
    *   `input_5.dat`: ~82 minutes

## Implemented Optimizations

### 1. Compiler Optimization Flags (`-O3`)
*   **Action:** Built using `CMAKE_BUILD_TYPE=Release`.
*   **Status:** Implemented (Baseline).
*   **Impact:** Provided a significant performance improvement over Debug builds, forming the baseline for further algorithmic tuning.

### 2. Adaptive Early Pass Termination
*   **Original Plan:** Phase 1.2 / 3.2 (Basic/Adaptive Algorithm Scope Limiting).
*   **Action:** Replaced a fixed move limit (`MAX_MOVES_WITHOUT_IMPROVEMENT`) with an adaptive threshold in `FMEngine::runPass`, decreasing from 2000 to 500 over passes.
*   **Status:** Implemented.
*   **Impact:** Dramatic reduction in runtime (~3.5 mins for `input_5.dat` initially), especially on benchmarks with long passes yielding little improvement. **Note:** This initially caused a regression on `input_0.dat` due to subtle issues with state restoration.

### 3. Cut Size Recalculation Fix (Related to Adaptive Termination)
*   **Action:** Modified `FMEngine::revertMovesToBestState` to explicitly recalculate the final cut size using `calculateCurrentCutSize()` after reverting moves, instead of relying solely on `moveHistory_`.
*   **Status:** Implemented.
*   **Impact:** Fixed the regression caused by adaptive termination, ensuring correctness while retaining most of the performance gains.

### 4. Selective Gain Updates
*   **Original Plan:** Phase 3.1.
*   **Action:** Replaced the full gain recalculation for neighbors in `updateGainsAfterMove` with inline, incremental gain updates within `FMEngine::applyMove`. Updates only occur for unlocked neighbors connected to nets whose critical state changes (based on partition counts before/after the move).
*   **Status:** Implemented.
*   **Impact:** Massive performance improvement, addressing the primary bottleneck. Runtimes dropped significantly again:
    *   `input_0.dat`: ~11 min (baseline) -> ~15 sec
    *   `input_5.dat`: ~82 min (baseline) -> ~10 sec

### 5. Reduced Logging Overhead
*   **Original Plan:** Phase 1.1.
*   **Action:** Commented out verbose `std::cout` statements in `Netlist` methods (`addCell`, `addNet`, `addCellToNet`), `FMEngine::applyMove`, and `FMEngine::undoMove`.
*   **Status:** Implemented.
*   **Impact:** Primarily cleaned up test execution output. Direct performance impact likely minor compared to algorithmic changes, but good practice.

## Potential Future Optimizations (Remaining Hotspots)

The most significant bottleneck (gain updates) has been addressed. Further optimizations might yield smaller returns but could still be valuable.

### 1. Memory Pre-allocation (Original Plan: Phase 2.2)
*   **Potential Action:** Use `reserve()` on key `std::vector` members (`Netlist::cells_`, `Netlist::nets_`, `FMEngine::moveHistory_`, potentially internal GainBucket structures) based on initial input size or reasonable estimates.
*   **Goal:** Reduce overhead from dynamic vector resizing during runtime, especially during parsing and the move history tracking.
*   **Expected Impact:** Modest reduction in allocation overhead, potentially more noticeable on very large benchmarks during initialization and pass execution.

### 2. Bucket List Implementation Review (Original Plan: Phase 2.1)
*   **Potential Action:** Review the `GainBucket` implementation (`DataStructures/GainBucket.cpp`). The current doubly-linked list approach with direct pointers from `Cell` structs *should* provide O(1) updates. Verify this holds true and there are no hidden inefficiencies.
*   **Goal:** Ensure the core data structure for selecting the best cell is maximally efficient.
*   **Expected Impact:** Likely low if current implementation is correct, but worth a quick verification.

### 3. Data Locality Improvements (Original Plan: Phase 2.3)
*   **Potential Action:** Analyze memory access patterns. Consider reordering fields within the `Cell` and `Net` structs to group frequently accessed data together. Potentially explore Structure of Arrays (SOA) vs. Array of Structures (AOS) for specific performance-critical loops (less likely to be needed now).
*   **Goal:** Improve cache utilization.
*   **Expected Impact:** Potentially moderate, but requires careful profiling and implementation effort.

### 4. Streamlined Move Application (Original Plan: Phase 3.3)
*   **Potential Action:** Review the `applyMove` function again. Although heavily modified for selective gain updates, check if any minor redundant calculations or operations remain.
*   **Goal:** Micro-optimization of the core move logic.
*   **Expected Impact:** Likely low, as the major work (gain update) has been optimized.