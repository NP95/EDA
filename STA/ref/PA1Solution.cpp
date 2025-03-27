#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <string>
#include <stdlib.h>
#include <unordered_map>
#include <set>
#include <queue>
#include <climits>
#include <iomanip>
#include "debug.hpp"  // Include debug header
#include <cstdlib>    // For getenv

using namespace std;

struct gate 
{           
    double delayTable[7][7];   
    double slewValues[7][7]; 
    double capacitance;
    vector<double> inputSlew;
    vector<double> outputLoad;
};

struct cktGates {
    string nodeType;
    vector<int> fanInList; 
    vector<int> fanOutList;
    double arrivalTime = 0;
    double inputSlew = 2;
    double slack = 0;
    double requiredArrivalTime = static_cast<double>(INT_MAX);
    bool isPrimaryOutput = false;
    bool isPrimaryInput = false;
};

struct ParenCommaEq_is_space : std::ctype<char> {
    ParenCommaEq_is_space() : std::ctype<char>(get_table()) {}
    static mask const* get_table() {
        static mask rc[table_size];
        rc['('] = std::ctype_base::space;
        rc[')'] = std::ctype_base::space;
        rc[','] = std::ctype_base::space;
        rc['='] = std::ctype_base::space;
        rc[' '] = std::ctype_base::space;
        rc['\t'] = std::ctype_base::space;
        rc['\r'] = std::ctype_base::space;
        return &rc[0];
    }
};

int library(char *fName, unordered_map<string, gate> &gates)
{
    Debug::detail("Starting library parsing from file: " + std::string(fName));
    ifstream ifs(fName);  
    bool cap = false;
    bool cellDelay = false;
    bool index1 = false;
    bool index2 = false;
    bool insideValues = false;
    bool insideSlewValues = false;
    
    string line;           
    gate currGate;  
    string gateName;
    int row = 0;
    int gateCount = 0;

    while (getline(ifs, line)) 
    {
        if (line.empty()) {
            continue;           
        }

        if (line.find("cell ") != string::npos) 
        {
            if (!gateName.empty()) {
                gates[gateName] = currGate;
                gateCount++;
                Debug::trace("Parsed gate: " + gateName);
            }
            gateName.clear();
            row = 0;

            currGate.inputSlew.clear();
            currGate.outputLoad.clear();

            char start;
            string cellName;
            istringstream iss(line);
            iss >> cellName >> start >> cellName;
            gateName = cellName.substr(0, cellName.find(')'));
            cap = true;
        }

        if(line.find("capacitance") != string::npos && cap) 
        {
            int pos = line.find(":");
            if(pos != string::npos)
            {
                string valString = line.substr(pos+1);
                valString.erase(remove(valString.begin(), valString.end(), ','), valString.end());
                currGate.capacitance = stod(valString);
                Debug::trace("Gate " + gateName + " capacitance: " + valString + " fF");
                cap = false;
            }
        }

        if(line.find("cell_delay") != string::npos) 
        {
            cellDelay = true;
            index1 = true;
        } 

        if(line.find("index_1 (") != string::npos && index1)
        {
            line = line.substr(line.find("index_1 (" + 8));
            line.erase(remove(line.begin(), line.end(), '('), line.end());
            line.erase(remove(line.begin(), line.end(), '\"'), line.end());
            line.erase(remove(line.begin(), line.end(), ')'), line.end());
            istringstream valuestream(line);
            string value;
            while(getline(valuestream, value, ','))
            {
                if(value.size() > 0)
                {
                    currGate.inputSlew.push_back(stod(value));
                }
            }
            Debug::trace("Parsed input slew indices for gate: " + gateName);
            index1 = false;
            index2 = true;
        }

        if(line.find("index_2 (") != string::npos && index2)
        {
            line = line.substr(line.find("index_2 (" + 8));
            line.erase(remove(line.begin(), line.end(), '('), line.end());
            line.erase(remove(line.begin(), line.end(), '\"'), line.end());
            line.erase(remove(line.begin(), line.end(), ')'), line.end());
            istringstream valuestream(line);
            string value;
            while(getline(valuestream, value, ','))
            {
                if(value.size() > 0)
                {
                    currGate.outputLoad.push_back(stod(value));
                }
            }
            Debug::trace("Parsed output load indices for gate: " + gateName);
            index2 = false;
        }

        if (line.find("values (") != string::npos && cellDelay) 
        {
            line = line.substr(line.find("values (") + 8);
            insideValues = true; 
        }

        if (insideValues) {
            line.erase(remove(line.begin(), line.end(), '\"'), line.end());
            line.erase(remove(line.begin(), line.end(), '\\'), line.end());
            istringstream valuestream(line);
            string value;
            int col = 0;
            while (getline(valuestream, value, ',')) {
                value.erase(remove(value.begin(), value.end(), ' '), value.end()); 
                if (value.size() > 0) {
                    if (row < 7 && col < 7) {
                        currGate.delayTable[row][col] = stod(value);  
                        col++;
                    }
                }
            }

            if (line.find(");") != string::npos) 
            {
                insideValues = false;
                cellDelay = false;
                Debug::trace("Parsed delay table for gate: " + gateName);
            }
            row++;  
        }

        if (line.find("values (") != string::npos && !cellDelay && !insideValues) {
            line = line.substr(line.find("values (") + 8);
            insideSlewValues = true; 
            row = 0;
        }

        if (insideSlewValues) {
            line.erase(remove(line.begin(), line.end(), '\"'), line.end());
            line.erase(remove(line.begin(), line.end(), '\\'), line.end());
            istringstream valuestream(line);
            string value;
            int col = 0;
            while (getline(valuestream, value, ',')) {
                value.erase(remove(value.begin(), value.end(), ' '), value.end()); 
                if (value.size() > 0) {
                    if (row < 7 && col < 7) {
                        currGate.slewValues[row][col] = stod(value);  
                        col++;
                    }
                }
            }

            if (line.find(");") != string::npos) {
                insideSlewValues = false;
                Debug::trace("Parsed slew table for gate: " + gateName);
            }
            row++;  
        }
    }

    if (!gateName.empty()) {
        gates[gateName] = currGate;
        gateCount++;
        Debug::trace("Parsed gate: " + gateName);
    }

    Debug::info("Library parsing complete. Loaded " + std::to_string(gateCount) + " gates.");
    ifs.close(); 
    return 0;
}

bool isEmptyOrWhitespace(const string& str) {
    return str.empty() || all_of(str.begin(), str.end(), ::isspace);
}

int parsingCircuitFile(string &cktfile, unordered_map<int, cktGates> &netlist) {
    Debug::detail("Starting circuit parsing from file: " + cktfile);
    ifstream ifs(cktfile);
    string line;
    int inputCount = 0;
    int outputCount = 0;
    int gateCount = 0;
    int dffCount = 0;

    while (getline(ifs, line)) {
        cktGates myGate;
        int nodeID;
        if (isEmptyOrWhitespace(line) || line[0] == '#') continue;
        istringstream iss(line);
        iss.imbue(locale(cin.getloc(), new ParenCommaEq_is_space)); 
        string nodeStr;
        iss >> nodeStr;

        if (nodeStr.find("INPUT") != string::npos) {
            iss >> nodeStr;
            nodeID = stoi(nodeStr);
            netlist[nodeID].isPrimaryInput = true;
            netlist[nodeID].nodeType = "INPUT";
            inputCount++;
            Debug::trace("Parsed INPUT: " + nodeStr);
        }
        else if (nodeStr.find("OUTPUT") != string::npos) {
            iss >> nodeStr;
            nodeID = stoi(nodeStr);
            netlist[nodeID].isPrimaryOutput = true;
            netlist[nodeID].nodeType = "OUTPUT";
            outputCount++;
            Debug::trace("Parsed OUTPUT: " + nodeStr);
        }
        else if (line.find("DFF") != string::npos) {
            int dffInputID = stoi(nodeStr); // DFF Input
            iss >> nodeStr; // Skip "DFF"
            iss >> nodeStr; // DFF Output ID in parentheses
            int dffOutputID = stoi(nodeStr);

            // Assign DFF nodes with appropriate connections
            netlist[dffOutputID].nodeType = "OUTPUT";
            netlist[dffOutputID].isPrimaryOutput = false;
            netlist[dffInputID].nodeType = "INPUT";
            netlist[dffInputID].isPrimaryInput = false;
            dffCount++;
            Debug::trace("Parsed DFF with input " + std::to_string(dffInputID) + " and output " + std::to_string(dffOutputID));
        }
        else {
            nodeID = stoi(nodeStr);
            iss >> nodeStr;
            transform(nodeStr.begin(), nodeStr.end(), nodeStr.begin(), ::toupper);
            netlist[nodeID].nodeType = nodeStr;
            gateCount++;

            std::string fanins;
            while (iss >> nodeStr) {
                int fanInNode = stoi(nodeStr);
                netlist[nodeID].fanInList.push_back(fanInNode);
                netlist[fanInNode].fanOutList.push_back(nodeID);
                fanins += nodeStr + " ";
            }
            Debug::trace("Parsed gate " + std::to_string(nodeID) + " of type " + netlist[nodeID].nodeType + " with fanins: " + fanins);
        }
    }

    Debug::info("Circuit parsing complete. Found " + std::to_string(inputCount) + " inputs, " + 
                std::to_string(outputCount) + " outputs, " + std::to_string(gateCount) + " gates, " +
                std::to_string(dffCount) + " DFFs");
    ifs.close();
    return 0;
}

unordered_map<int, vector<int>> circuitGraph(unordered_map<int, cktGates> &netlist) {
    unordered_map<int, vector<int>> adjacencyList;
    for(const auto &[nodeID, gate] : netlist) {
        adjacencyList[nodeID] = gate.fanOutList;
    }
    return adjacencyList;
}

void topologicalSort(const unordered_map<int, cktGates> &netlist, vector<int> &sortedList) {
    Debug::detail("Starting topological sort...");
    unordered_map<int, int> inDegree;
    for(const auto &[nodeID, node] : netlist) {
        inDegree[nodeID] = node.fanInList.size();
    }

    queue<int> q;
    for(const auto &[nodeID, node] : netlist) {
        if(inDegree[nodeID] == 0) {
            q.push(nodeID);
            Debug::trace("Adding zero-indegree node to queue: " + std::to_string(nodeID));
        }
    }

    while(!q.empty()) {
        int node = q.front();
        q.pop();
        sortedList.push_back(node);
        Debug::trace("Adding node to sorted list: " + std::to_string(node));
        
        for(int fanOut : netlist.at(node).fanOutList) {
            if(--inDegree[fanOut] == 0) {
                q.push(fanOut);
                Debug::trace("  Fanout " + std::to_string(fanOut) + " now has zero indegree, adding to queue");
            }
        }
    }
    
    Debug::info("Topological sort complete. Sorted " + std::to_string(sortedList.size()) + " nodes.");
}

double interpolate(double inputSlew, double loadCap, const gate &gateInfo, bool is_delay) {
    Debug::trace("==== Interpolation Start (" + std::string(is_delay ? "Delay" : "Slew") + ") ====");
    Debug::trace("  Inputs: slew=" + std::to_string(inputSlew) + " ps, load=" + std::to_string(loadCap) + " fF");
    
    const vector<double> &slewIndices = gateInfo.inputSlew;
    const vector<double> &capIndices = gateInfo.outputLoad;
    
    // Check for empty indices (error handling)
    if (slewIndices.empty() || capIndices.empty()) {
        Debug::error("Interpolate: Index vectors empty.");
        return 0.0;
    }
    
    double inputSlew_ns = inputSlew / 1000.0; // Convert to nanoseconds
    double original_slew_ns = inputSlew_ns; // Store original
    double original_load_fF = loadCap; // Store original

    // Find bounding indices for input slew
    auto slewIt = upper_bound(slewIndices.begin(), slewIndices.end(), inputSlew_ns);
    int slewIndex2 = distance(slewIndices.begin(), slewIt);
    int slewIndex1 = max(0, slewIndex2 - 1);
    slewIndex2 = min(static_cast<int>(slewIndices.size()) - 1, slewIndex2);
    
    // Apply clamping for calculation if needed
    if (inputSlew_ns <= slewIndices.front()) inputSlew_ns = slewIndices.front();
    else if (inputSlew_ns >= slewIndices.back()) inputSlew_ns = slewIndices.back();

    // Find bounding indices for load capacitance
    auto capIt = upper_bound(capIndices.begin(), capIndices.end(), loadCap);
    int capIndex2 = distance(capIndices.begin(), capIt);
    int capIndex1 = max(0, capIndex2 - 1);
    capIndex2 = min(static_cast<int>(capIndices.size()) - 1, capIndex2);
    
    // Apply clamping for calculation if needed
    if (loadCap <= capIndices.front()) loadCap = capIndices.front();
    else if (loadCap >= capIndices.back()) loadCap = capIndices.back();

    // Index validation
    if (slewIndex1 < 0 || slewIndex1 >= 7 || slewIndex2 < 0 || slewIndex2 >= 7 || 
        capIndex1 < 0 || capIndex1 >= 7 || capIndex2 < 0 || capIndex2 >= 7) {
        Debug::error("Interpolate: Invalid indices computed: s1=" + std::to_string(slewIndex1) + 
                    ", s2=" + std::to_string(slewIndex2) + ", c1=" + std::to_string(capIndex1) + 
                    ", c2=" + std::to_string(capIndex2));
        return 0.0;
    }

    // Retrieve values for bilinear interpolation
    double v11, v12, v21, v22;
    if (is_delay) {
        v11 = gateInfo.delayTable[slewIndex1][capIndex1];
        v12 = gateInfo.delayTable[slewIndex1][capIndex2];
        v21 = gateInfo.delayTable[slewIndex2][capIndex1];
        v22 = gateInfo.delayTable[slewIndex2][capIndex2];
    } else {
        v11 = gateInfo.slewValues[slewIndex1][capIndex1];
        v12 = gateInfo.slewValues[slewIndex1][capIndex2];
        v21 = gateInfo.slewValues[slewIndex2][capIndex1];
        v22 = gateInfo.slewValues[slewIndex2][capIndex2];
    }

    Debug::trace("  Table corners: v11=" + std::to_string(v11) + ", v12=" + std::to_string(v12) + 
                ", v21=" + std::to_string(v21) + ", v22=" + std::to_string(v22));

    // Get coordinates for interpolation
    double t1 = slewIndices[slewIndex1];
    double t2 = slewIndices[slewIndex2];
    double c1 = capIndices[capIndex1];
    double c2 = capIndices[capIndex2];

    Debug::trace("  Indices: slew[" + std::to_string(slewIndex1) + "]=" + std::to_string(t1) + 
                ", slew[" + std::to_string(slewIndex2) + "]=" + std::to_string(t2) + 
                ", cap[" + std::to_string(capIndex1) + "]=" + std::to_string(c1) + 
                ", cap[" + std::to_string(capIndex2) + "]=" + std::to_string(c2));

    // Perform bilinear interpolation with improved handling of edge cases
    double interpolatedValue_ns = 0.0;
    double delta_c = c2 - c1;
    double delta_t = t2 - t1;
    const double epsilon = 1e-12;

    if (std::fabs(delta_c) < epsilon && std::fabs(delta_t) < epsilon) {
        interpolatedValue_ns = v11;
        Debug::trace("  Using direct value (equal bounds)");
    } else if (std::fabs(delta_t) < epsilon) {
        interpolatedValue_ns = v11 + (v12 - v11) * (original_load_fF - c1) / delta_c;
        Debug::trace("  Using linear interpolation on load dimension only");
    } else if (std::fabs(delta_c) < epsilon) {
        interpolatedValue_ns = v11 + (v21 - v11) * (original_slew_ns - t1) / delta_t;
        Debug::trace("  Using linear interpolation on slew dimension only");
    } else {
        double term1 = v11 * (c2 - original_load_fF) * (t2 - original_slew_ns);
        double term2 = v12 * (original_load_fF - c1) * (t2 - original_slew_ns);
        double term3 = v21 * (c2 - original_load_fF) * (original_slew_ns - t1);
        double term4 = v22 * (original_load_fF - c1) * (original_slew_ns - t1);
        interpolatedValue_ns = (term1 + term2 + term3 + term4) / (delta_c * delta_t);
        Debug::trace("  Using full bilinear interpolation");
    }

    double finalResult_ps = interpolatedValue_ns * 1000.0; // Convert back to picoseconds
    Debug::trace("  Result: " + std::to_string(finalResult_ps) + " ps");
    Debug::trace("==== Interpolation End ====");

    return finalResult_ps;
}

double forwardTraversal(const vector<int> &sortedList, unordered_map<int, cktGates> &netlist, const unordered_map<string, gate> &gates) {
    Debug::info("Starting forward traversal...");
    double circuitDelay = 0;
    
    for (int node : sortedList) {
        cktGates& currentNode = netlist[node];
        std::string nodeStr = std::to_string(node);
        
        Debug::trace("Processing node forward: " + nodeStr + " (" + currentNode.nodeType + ")");
        
        if (currentNode.isPrimaryInput || (currentNode.nodeType == "OUTPUT" && !currentNode.isPrimaryOutput)) {
            Debug::trace("  Skipping node " + nodeStr + " (primary input or DFF output)");
            currentNode.arrivalTime = 0;
            currentNode.inputSlew = 2;
            continue;
        }

        if (currentNode.nodeType == "INPUT" && !currentNode.isPrimaryInput) {
            Debug::trace("  Node " + nodeStr + " is a DFF input, updating circuit delay");
            circuitDelay = max(circuitDelay, currentNode.arrivalTime);
            continue;
        }

        const gate &gateInfo = gates.at(currentNode.nodeType);
        double maxArrivalTime = 0;
        double maxInputSlew = 0;
        
        // Calculate load capacitance
        double loadCap = currentNode.isPrimaryOutput ? 4 * gates.at("INV").capacitance : 0;
        for (int fanOutNode : currentNode.fanOutList) {
            loadCap += gates.at(netlist[fanOutNode].nodeType).capacitance;
        }
        Debug::detail("  Node " + nodeStr + " LoadCap = " + std::to_string(loadCap) + " fF");

        Debug::detail("  Processing " + std::to_string(currentNode.fanInList.size()) + " fanins for node " + nodeStr);
        for (int fanInNode : currentNode.fanInList) {
            std::string faninStr = std::to_string(fanInNode);
            Debug::trace("    Processing fanin " + faninStr + 
                        " with AT=" + std::to_string(netlist[fanInNode].arrivalTime) + " ps, " +
                        "InSlew=" + std::to_string(netlist[fanInNode].inputSlew) + " ps");
            
            // Calculate delay and output slew
            double raw_delay = interpolate(netlist[fanInNode].inputSlew, loadCap, gateInfo, true);
            double outputSlew = interpolate(netlist[fanInNode].inputSlew, loadCap, gateInfo, false);
            
            // Apply scaling for multi-input gates
            double scaleFactor = 1.0;
            double final_delay = raw_delay;
            
            bool is1InputGate = (currentNode.nodeType == "INV" || currentNode.nodeType == "BUF" || 
                                currentNode.nodeType == "NOT" || currentNode.nodeType == "BUFF");
                                
            if (!is1InputGate && currentNode.fanInList.size() > 2) {
                scaleFactor = (currentNode.fanInList.size() / 2.0);
                final_delay *= scaleFactor;
                Debug::trace("    Applied scaling factor " + std::to_string(scaleFactor) + " to delay");
            }
            
            // Trace gate delay calculation
            Debug::trace("    Gate delay calculation: raw_delay=" + std::to_string(raw_delay) + 
                        " ps, final_delay=" + std::to_string(final_delay) + " ps");
            
            // Calculate arrival time
            double arrivalTime = netlist[fanInNode].arrivalTime + final_delay;
            Debug::trace("    AT from fanin " + faninStr + ": " + 
                        std::to_string(netlist[fanInNode].arrivalTime) + " + " + 
                        std::to_string(final_delay) + " = " + std::to_string(arrivalTime) + " ps");
            
            // Update max values if needed
            if (arrivalTime > maxArrivalTime) {
                maxArrivalTime = arrivalTime;
                maxInputSlew = outputSlew;
                Debug::trace("    Updated max values: AT=" + std::to_string(maxArrivalTime) + 
                            " ps, slew=" + std::to_string(maxInputSlew) + " ps");
            }
        }

        // Update node values
        currentNode.arrivalTime = maxArrivalTime;
        currentNode.inputSlew = maxInputSlew;
        Debug::detail("  Final values for node " + nodeStr + ": AT=" + 
                    std::to_string(currentNode.arrivalTime) + " ps, InSlew=" + 
                    std::to_string(currentNode.inputSlew) + " ps");

        // Update circuit delay if this is a primary output
        if (currentNode.isPrimaryOutput) {
            if (currentNode.arrivalTime > circuitDelay) {
                Debug::info("  Updated circuit delay to " + std::to_string(currentNode.arrivalTime) + 
                          " ps from output node " + nodeStr);
                circuitDelay = currentNode.arrivalTime;
            }
        }
    }
    
    Debug::info("Forward traversal complete. Circuit Delay = " + std::to_string(circuitDelay) + " ps");
    return circuitDelay;
}

void backwardTraversal(const vector<int>& sortedList, unordered_map<int, cktGates>& netlist, const unordered_map<string, gate>& gates, double circuitDelay) {
    double requiredTimeVal = 1.1 * circuitDelay; 
    Debug::info("Starting backward traversal with required time = " + std::to_string(requiredTimeVal) + " ps");

    // Initialize required times for primary outputs
    for (auto& [nodeID, nodeData] : netlist) {
        if (nodeData.isPrimaryOutput) {
            nodeData.requiredArrivalTime = requiredTimeVal;
            Debug::trace("Initialized PO node " + std::to_string(nodeID) + " with RAT=" + 
                       std::to_string(requiredTimeVal) + " ps");
        } else {
            nodeData.requiredArrivalTime = static_cast<double>(INT_MAX);
        }
    }

    // Traverse the sorted list in reverse to propagate required times
    for (auto it = sortedList.rbegin(); it != sortedList.rend(); ++it) {
        int node = *it;
        cktGates& currentNode = netlist[node];
        std::string nodeStr = std::to_string(node);
        
        Debug::trace("Processing node backward: " + nodeStr + " (" + currentNode.nodeType + ")");

        // Skip primary output nodes as they're already initialized
        if (currentNode.isPrimaryOutput) {
            currentNode.slack = currentNode.requiredArrivalTime - currentNode.arrivalTime;
            Debug::detail("  PO node " + nodeStr + ": RAT=" + 
                        std::to_string(currentNode.requiredArrivalTime) + " ps, Slack=" + 
                        std::to_string(currentNode.slack) + " ps");
            continue;
        }
        
        // Handle dead nodes (no fanouts)
        if (currentNode.fanOutList.empty()) {
            currentNode.requiredArrivalTime = std::numeric_limits<double>::infinity();
            currentNode.slack = std::numeric_limits<double>::infinity();
            Debug::detail("  Dead node " + nodeStr + ": RAT=inf, Slack=inf");
            continue;
        }

        double minRequiredTime = std::numeric_limits<double>::infinity();
        
        Debug::detail("  Processing " + std::to_string(currentNode.fanOutList.size()) + " fanouts for node " + nodeStr);
        for (int fanOutNode : currentNode.fanOutList) {
            const cktGates& fanOutGate = netlist[fanOutNode];
            std::string fanoutStr = std::to_string(fanOutNode);
            
            Debug::trace("    Processing fanout " + fanoutStr + " with RAT=" + 
                       std::to_string(fanOutGate.requiredArrivalTime) + " ps");
            
            // Special handling for OUTPUT nodes (e.g., DFF outputs)
            if (fanOutGate.nodeType == "OUTPUT") {
                minRequiredTime = std::min(minRequiredTime, fanOutGate.requiredArrivalTime);
                Debug::trace("    OUTPUT fanout: updated minRAT to " + std::to_string(minRequiredTime) + " ps");
                continue;
            }
            
            const gate& fanOutGateInfo = gates.at(fanOutGate.nodeType);
            
            // Calculate load capacitance for fanout
            double loadCap = fanOutGate.isPrimaryOutput ? 4 * gates.at("INV").capacitance : 0;
            for (int nextFanOutNode : fanOutGate.fanOutList) {
                loadCap += gates.at(netlist[nextFanOutNode].nodeType).capacitance;
            }
            Debug::trace("    LoadCap for fanout " + fanoutStr + " = " + std::to_string(loadCap) + " fF");
            
            // Calculate delay through fanout gate
            double delay = interpolate(currentNode.inputSlew, loadCap, fanOutGateInfo, true);
            Debug::trace("    Raw delay through fanout " + fanoutStr + ": " + std::to_string(delay) + " ps");
            
            // Apply scaling for multi-input gates
            double scaleFactor = 1.0;
            bool is1InputGate = (fanOutGate.nodeType == "INV" || fanOutGate.nodeType == "BUF" || 
                               fanOutGate.nodeType == "NOT" || fanOutGate.nodeType == "BUFF");
                               
            if (!is1InputGate && fanOutGate.fanInList.size() > 2) {
                scaleFactor = (fanOutGate.fanInList.size() / 2.0);
                delay *= scaleFactor;
                Debug::trace("    Applied scaling factor " + std::to_string(scaleFactor) + " to delay");
            }
            
            // Calculate required time contribution
            double requiredTime = fanOutGate.requiredArrivalTime - delay;
            Debug::trace("    RAT contrib from " + fanoutStr + ": " + 
                       std::to_string(fanOutGate.requiredArrivalTime) + " - " + 
                       std::to_string(delay) + " = " + std::to_string(requiredTime) + " ps");
            
            // Update minimum required time
            minRequiredTime = std::min(minRequiredTime, requiredTime);
        }

        // Set required time (handle case where no path to output)
        if (std::isinf(minRequiredTime)) {
            currentNode.requiredArrivalTime = std::numeric_limits<double>::infinity();
            Debug::detail("  Node " + nodeStr + " has no valid paths to outputs, setting RAT=inf");
        } else {
            currentNode.requiredArrivalTime = minRequiredTime;
        }

        // Calculate slack with special handling for primary inputs
        if (currentNode.isPrimaryInput) {
            // For primary inputs, calculate slack directly
            currentNode.slack = currentNode.requiredArrivalTime - currentNode.arrivalTime;
            Debug::detail("  PI node " + nodeStr + ": RAT=" + 
                        std::to_string(currentNode.requiredArrivalTime) + " ps, Slack=" + 
                        std::to_string(currentNode.slack) + " ps");
        } else {
            currentNode.slack = currentNode.requiredArrivalTime - currentNode.arrivalTime;
            Debug::detail("  Node " + nodeStr + ": RAT=" + 
                        std::to_string(currentNode.requiredArrivalTime) + " ps, Slack=" + 
                        std::to_string(currentNode.slack) + " ps");
        }
    }
    
    Debug::info("Backward traversal complete.");
}

vector<int> CriticalPath(const unordered_map<int, cktGates> &netlist) {
    Debug::info("Identifying critical path...");
    vector<int> criticalPath;
    int currentNode = -1;
    double maxDelay = 0;

    Debug::detail("Finding primary output with maximum delay");
    for (const auto &[nodeID, node] : netlist) {
        if (node.isPrimaryOutput) {
            Debug::trace("  PO " + std::to_string(nodeID) + " AT=" + std::to_string(node.arrivalTime) + " ps");
            if (node.arrivalTime > maxDelay) {
                maxDelay = node.arrivalTime;
                currentNode = nodeID;
                Debug::trace("    New max delay found: " + std::to_string(maxDelay) + " ps at node " + std::to_string(currentNode));
            }
        }
    }

    if (currentNode == -1) {
        Debug::warn("No critical path found - no primary outputs?");
        return criticalPath;
    }
    
    Debug::detail("Starting critical path from output " + std::to_string(currentNode) + 
                " with delay " + std::to_string(maxDelay) + " ps");

    while (currentNode != -1) {
        criticalPath.push_back(currentNode);
        Debug::trace("Added node " + std::to_string(currentNode) + " to critical path");

        if (netlist.at(currentNode).isPrimaryInput) {
            Debug::trace("Reached primary input " + std::to_string(currentNode));
            break;
        }

        int prevNode = -1;
        double maxPrevArrivalTime = -1;
        Debug::trace("Finding fanin with maximum arrival time for node " + std::to_string(currentNode));
        
        for (int fanInNode : netlist.at(currentNode).fanInList) {
            Debug::trace("  Checking fanin " + std::to_string(fanInNode) + 
                       " with AT=" + std::to_string(netlist.at(fanInNode).arrivalTime) + " ps");
                       
            if (netlist.at(fanInNode).arrivalTime > maxPrevArrivalTime) {
                maxPrevArrivalTime = netlist.at(fanInNode).arrivalTime;
                prevNode = fanInNode;
                Debug::trace("    Updated maxAT to " + std::to_string(maxPrevArrivalTime) + 
                           " ps from fanin " + std::to_string(prevNode));
            }
        }
        
        if (prevNode != -1) {
            Debug::detail("Selected fanin " + std::to_string(prevNode) + 
                        " for critical path with AT=" + std::to_string(maxPrevArrivalTime) + " ps");
        } else {
            Debug::warn("Could not find critical fanin for node " + std::to_string(currentNode));
        }
        
        currentNode = prevNode;
    }

    reverse(criticalPath.begin(), criticalPath.end());
    
    // Log final path
    std::stringstream pathStr;
    for (size_t i = 0; i < criticalPath.size(); ++i) {
        pathStr << criticalPath[i];
        if (i < criticalPath.size() - 1) pathStr << " -> ";
    }
    Debug::info("Identified critical path with " + std::to_string(criticalPath.size()) + 
              " nodes: " + pathStr.str());
              
    return criticalPath;
}

int main(int argc, char *argv[]) {
    // Initialize debug based on environment variable
    Debug::Level debugLevel = Debug::INFO; // Default level
    const char* levelStrEnv = std::getenv("STA_DEBUG_LEVEL");
    if (levelStrEnv) {
        std::string levelStr = levelStrEnv;
        if (levelStr == "TRACE") debugLevel = Debug::TRACE;
        else if (levelStr == "DETAIL") debugLevel = Debug::DETAIL;
        else if (levelStr == "INFO") debugLevel = Debug::INFO;
        else if (levelStr == "WARN") debugLevel = Debug::WARN;
        else if (levelStr == "ERROR") debugLevel = Debug::ERROR;
        else if (levelStr == "NONE") debugLevel = Debug::NONE;
    }
    
    std::string logFileName = "sta_debug.log";
    Debug::initialize(debugLevel, logFileName);
    
    Debug::info("Starting Static Timing Analysis...");
    
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <library_file> <circuit_file>" << endl;
        Debug::error("Insufficient command line arguments");
        Debug::cleanup();
        return 1;
    }
    
    // Set circuit name for debugging
    if (argc >= 3) {
        Debug::setCircuitName(argv[2]);
    }
    
    Debug::info("Reading library from: " + std::string(argv[1]));
    Debug::info("Reading circuit from: " + std::string(argv[2]));
    
    unordered_map<string, gate> gates;
    library(argv[1], gates);

    unordered_map<int, cktGates> netlist;
    string cktfile = argv[2];
    parsingCircuitFile(cktfile, netlist);

    vector<int> sortedList;
    topologicalSort(netlist, sortedList);

    double circuitDelay = forwardTraversal(sortedList, netlist, gates);
    backwardTraversal(sortedList, netlist, gates, circuitDelay);

    vector<int> criticalPath = CriticalPath(netlist);

    Debug::info("Writing output to ckt_traversal.txt");
    ofstream outFile("ckt_traversal.txt");
    outFile << fixed << setprecision(2);
    outFile << "Circuit delay: " << circuitDelay << " ps\n\n";
    outFile << "Gate slacks:\n";

    vector<pair<int, cktGates>> sortedNetlist(netlist.begin(), netlist.end());
    sort(sortedNetlist.begin(), sortedNetlist.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first < rhs.first;
    });

    for (const auto& [nodeID, node] : sortedNetlist) {
        string prefix = node.isPrimaryInput ? "INP-" : (node.isPrimaryOutput ? "OUT-" : node.nodeType + "-");
        outFile << prefix << "n" << nodeID << ": " << node.slack << " ps\n";
    }

    outFile << "\nCritical path:\n";
    for (size_t i = 0; i < criticalPath.size(); ++i) {
        int nodeID = criticalPath[i];
        const auto& node = netlist.at(nodeID);
        string prefix = node.isPrimaryInput ? "INP-" : (node.isPrimaryOutput ? "OUT-" : node.nodeType + "-");
        outFile << prefix << "n" << nodeID;
        if (i < criticalPath.size() - 1) outFile << ", ";
    }
    outFile << "\n";

    outFile.close();
    
    Debug::info("STA completed successfully!");
    Debug::cleanup();
    return 0;
}