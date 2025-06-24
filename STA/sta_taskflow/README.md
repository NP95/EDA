# Taskflow-based Static Timing Analysis

This project implements a parallel Static Timing Analysis (STA) tool using Taskflow for task-based parallelism. The implementation maintains bit-identical results with the reference sequential implementation while achieving significant performance improvements.

## Requirements

- C++17 or later compiler (g++ 8.4+, clang 6.0+, MSVC 2019+)
- CMake 3.14 or later
- Taskflow (included as a submodule)

## Building

```bash
# Clone the repository with submodules
git clone --recursive https://github.com/yourusername/sta_taskflow.git
cd sta_taskflow

# Create build directory
mkdir build
cd build

# Configure and build
cmake ..
make -j$(nproc)
```

## Usage

```bash
./sta <library_file> <circuit_file>
```

Example:
```bash
./sta ../test/NLDM_lib_max2Inp ../test/cleaned_iscas89_99_circuits/c17.isc
```

## Output Format

The tool generates a `ckt_traversal.txt` file with the following format:

```
Circuit delay: <delay> ps

Gate slacks:
<gate_type>-n<node_id>: <slack> ps
...

Critical path:
<gate_type>-n<node_id>, <gate_type>-n<node_id>, ...
```

## Validation

To validate the implementation against the reference:

```bash
make validate
```

## Performance Measurement

To measure performance improvements:

```bash
make performance
```

## Implementation Details

- Uses Taskflow for task-based parallelism
- Maintains bit-identical results with sequential implementation
- Supports netlists with up to 150,000 gates
- Handles 7x7 NLDM lookup tables with bilinear interpolation
- Processes flip-flops by splitting them into input/output nodes

## License

This project is licensed under the MIT License - see the LICENSE file for details. 