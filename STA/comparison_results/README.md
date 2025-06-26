# STA Taskflow vs Reference Comparison Results

## Directory Structure

```
comparison_results/
├── README.md                 # This file
├── summary_delays.txt        # Consolidated comparison table
├── sta_taskflow/            # Results from sta_taskflow implementation
│   ├── delays/              # Circuit delay values (one per file)
│   │   ├── c17_delay.txt
│   │   ├── c1908__delay.txt
│   │   ├── c2670_delay.txt
│   │   ├── c3540_delay.txt
│   │   ├── c5315_delay.txt
│   │   ├── c7552_delay.txt
│   │   ├── b15_1_delay.txt
│   │   ├── b18_1_delay.txt
│   │   ├── b19_1_delay.txt
│   │   └── b22_delay.txt
│   └── critical_paths/      # Critical path sequences (one per file)
│       ├── c17_path.txt
│       ├── c1908__path.txt
│       ├── c2670_path.txt
│       ├── c3540_path.txt
│       ├── c5315_path.txt
│       ├── c7552_path.txt
│       ├── b15_1_path.txt
│       ├── b18_1_path.txt
│       ├── b19_1_path.txt
│       └── b22_path.txt
└── reference/               # Results from reference implementation
    ├── delays/              # Circuit delay values (one per file)
    │   ├── c17_delay.txt
    │   ├── c1908_delay.txt
    │   ├── c2670_delay.txt
    │   ├── c3540_delay.txt
    │   ├── c5315_delay.txt
    │   ├── c7552_delay.txt
    │   ├── b15_delay.txt
    │   ├── b18_delay.txt
    │   ├── b19_delay.txt
    │   └── b22_delay.txt
    └── critical_paths/      # Critical path sequences (one per file)
        ├── c17_path.txt
        ├── c1908_path.txt
        ├── c2670_path.txt
        ├── c3540_path.txt
        ├── c5315_path.txt
        ├── c7552_path.txt
        ├── b15_path.txt
        ├── b18_path.txt
        ├── b19_path.txt
        └── b22_path.txt
```

## File Contents

### Delay Files
Each delay file contains a single line with format:
```
Circuit delay: XXXX.XX ps
```

### Critical Path Files  
Each path file contains a single line with format:
```
Critical path: node1, node2, node3, ..., nodeN
```

## Key Findings

1. **Overall Performance**: sta_taskflow consistently underestimates delays by 1-19% compared to reference
2. **Critical Path Divergence**: 
   - 'c' series circuits: High divergence (0.81-1.00) - completely different paths
   - 'b' series circuits: Low divergence (0.00-0.02) - nearly identical paths
3. **Accuracy**: c17 shows perfect match (0.00% error), while c1908 shows highest error (-18.89%)

## Usage

- Compare delay values: `diff sta_taskflow/delays/c17_delay.txt reference/delays/c17_delay.txt`
- Compare critical paths: `diff sta_taskflow/critical_paths/c17_path.txt reference/critical_paths/c17_path.txt`
- View summary: `cat summary_delays.txt`

## Data Source

- STA Taskflow results: Generated from `/home/nishant/EDA/STA/sta_taskflow/results/`
- Reference results: Copied from `/home/nishant/EDA/STA/Assignment_ref/results/` 