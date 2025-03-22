# Static Timing Analysis (STA) Project

This project implements a **Static Timing Analysis (STA)** tool to calculate circuit delay, slack values, and identify the critical path in a digital circuit. The program handles standard liberty files (.lib) and circuit netlist files to perform comprehensive timing analysis.

---

## **Flow Overview**

### **1. Input Files**
- **Liberty File**: Describes the gate delay and slew behavior using Look-Up Tables (LUTs).
- **Netlist File**: Describes the circuit structure as a directed graph with gates, interconnections, primary inputs, and primary outputs.

The program expects the liberty file and netlist file as command-line arguments:
```
./sta <liberty_file> <circuit_file>
```

---

### **2. Parsing the Liberty File**
- **Function**: `library()`
- **Purpose**: Extracts delay and slew LUTs, input slew, output load indices, and capacitance values for gates.
- **Steps**:
  1. Identify each gate in the liberty file.
  2. Parse indices for input slew and output load.
  3. Extract delay and slew LUT values using row-column mapping.
  4. Store parsed data in a map `unordered_map<string, gate>` for lookup.
 
   <div align="center" style="margin-top: 20px; margin-bottom: 20px;">
  <img src="https://github.com/user-attachments/assets/354a4deb-c357-4220-9eef-42025f055fb4" alt="Image description">
</div>

---

### **3. Parsing the Netlist File**
- **Function**: `parsingCircuitFile()`
- **Purpose**: Builds a directed graph representation of the circuit.
- **Steps**:
  1. Parse nodes (gates) and connections (fan-in and fan-out lists).
  2. Identify primary inputs and primary outputs.
  3. Handle flip-flops (DFFs) by splitting them into input and output nodes.
  4. Store the parsed data in a map `unordered_map<int, cktGates>`.

---

### **4. Topological Sorting**
- **Function**: `topologicalSort()`
- **Purpose**: Generates a valid processing order of gates based on their dependencies.
- **Steps**:
  1. Calculate in-degrees for all gates.
  2. Use a queue to process gates with zero in-degrees.
  3. Generate a sorted list of gate IDs.

---

### **5. Forward Traversal (Arrival Time Calculation)**
- **Function**: `forwardTraversal()`
- **Purpose**: Calculates the arrival time at each gate and determines the total circuit delay.
- **Steps**:
  1. For each gate in topological order:
     - Calculate load capacitance based on fan-out gates.
     - Use bilinear interpolation on LUTs to compute delay and output slew.
     - Update the gate's arrival time as the maximum arrival time from its fan-in nodes plus the computed delay.
  2. Track the maximum arrival time at primary outputs to determine the circuit delay.
 
<div align="center" style="margin-top: 20px; margin-bottom: 20px;">
  <img src="https://github.com/user-attachments/assets/6271e805-dbe6-45d1-a032-404c8b913594" alt="Image description">
</div>

#### **Maximum Function Used**:
The arrival time for a gate is calculated as:
```
arrival_time = max(arrival_time_fan_in + delay)
```
Where `delay` is obtained through bilinear interpolation.

---

### **6. Backward Traversal (Slack Calculation)**
- **Function**: `backwardTraversal()`
- **Purpose**: Calculates slack for each gate by propagating required arrival times backward from the primary outputs.
- **Steps**:
  1. Initialize required arrival time at primary outputs as `1.1 * circuit delay`.
  2. For each gate in reverse topological order:
     - Calculate the required arrival time based on fan-out gates and their delays.
     - Compute slack as the difference between required and actual arrival times.

#### **Minimum Function Used**:
The required arrival time for a gate is calculated as:
```
required_time = min(required_time_fan_out - delay)
```
Slack is then computed as:
```
slack = required_time - arrival_time
```

---

### **7. Bilinear Interpolation Equation**
**Purpose**: To compute delay and slew values from LUTs for intermediate input slew and load capacitance values.

Given values from the LUT:
- `v11`, `v12`, `v21`, `v22`: Delay or slew values at bounding points.
- `t1`, `t2`: Slew indices.
- `c1`, `c2`: Capacitance indices.

The interpolated value is calculated as:
```
v = ((v11 * (c2 - C) * (t2 - T)) + 
     (v12 * (C - c1) * (t2 - T)) + 
     (v21 * (c2 - C) * (T - t1)) + 
     (v22 * (C - c1) * (T - t1))) / 
    ((c2 - c1) * (t2 - t1))
```
Where:
- `C`: Load capacitance.
- `T`: Input slew.

The final interpolated delay or slew is then scaled as needed (e.g., converting units).

---

### **8. Critical Path Extraction**
- **Function**: `CriticalPath()`
- **Purpose**: Identifies the sequence of gates with the largest arrival time.
- **Steps**:
  1. Start from the primary output with the maximum arrival time.
  2. Trace backward through fan-in nodes with the largest arrival times.
  3. Generate and output the critical path.

---

### **9. Output**
The program generates a file `ckt_traversal.txt` with the following details:
- **Circuit Delay**: Total circuit delay in picoseconds.
- **Gate Slacks**: Slack values for each gate.
- **Critical Path**: Sequence of gates in the critical path.

Sample format:
```
Circuit delay: <value> ps

Gate slacks:
<GateType>-n<ID>: <slack> ps
...

Critical path:
<GateType>-n<ID>, <GateType>-n<ID>, ...
```

---

## **Key Features**
- Handles circuits with up to 150,000 gates efficiently.
- Supports bilinear interpolation for accurate delay and slew calculations.
- Automatically detects and processes primary inputs, outputs, and flip-flops.
- Provides precise slack calculations for all gates.

---

## **Execution Instructions**
1. Compile the program using the provided Makefile:
   ```
make
   ```
2. Run the program with the liberty and netlist files as inputs:
   ```
./sta <liberty_file> <circuit_file>
   ```
3. Check the output in `ckt_traversal.txt` for results.

---

## **Dependencies**
- C++ Standard Library
- Linux environment for compilation and execution

---

## **Limitations**
- Assumes liberty file contains only 2-input gates.
- Simplified delay scaling for gates with more than 2 inputs.
- Ignores wire capacitance in load calculations.

---

Feel free to reach out for any clarifications or improvements!

