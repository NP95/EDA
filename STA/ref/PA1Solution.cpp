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

    while (getline(ifs, line)) 
    {

        if (line.empty()) {
            continue;           
        }

        if (line.find("cell ") != string::npos) 
        {
            if (!gateName.empty()) {
                gates[gateName] = currGate;
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
                //std::cout << "Extracted capacitance value: " << valString << std::endl;
                currGate.capacitance = stod(valString);
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
                    //std::cout << "Extracted inputSlew value: " << value << std::endl;
                    currGate.inputSlew.push_back(stod(value));
                }
            }
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
                    //std::cout << "Extracted outputLoad value: " << value << std::endl;
                    currGate.outputLoad.push_back(stod(value));
                }
            }
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
            }
            row++;  
        }
    }

    if (!gateName.empty()) {
        gates[gateName] = currGate;
    }

    ifs.close(); 
    return 0;
}

// void printingData(unordered_map<string, gate> &gates, const string &gateName)
// {
//     auto it = gates.find(gateName);
//     if(it != gates.end())
//     {
//         gate &G = it -> second;
//         cout << "Delay Table for " << gateName << endl;
//         cout << endl;
//         for (int i = 0; i < 7; i++) {
//             for (int j = 0; j < 7; j++) {
//                 cout << G.delayTable[i][j] << (j < 6 ? ", " : ";\n");
//             }
//         }
//         cout << endl;

//         cout << "Slew Values for " << gateName << endl;
//         cout << endl;
//         for (int i = 0; i < 7; i++) {
//             for (int j = 0; j < 7; j++) {
//                 cout << G.slewValues[i][j] << (j < 6 ? ", " : ";\n");
//             }
//         }
//         cout << endl;

//         cout << "Capacitance for " << gateName << endl;
//         cout << endl;
//         cout << G.capacitance;
//         cout << endl;

//         cout << endl;
//         cout << "Input Slew for " << gateName << endl;
//         cout << endl;
//         for(double i : G.inputSlew)
//         {
//             cout << i << ", ";
//         }
//         cout << endl;

//         cout << endl;
//         cout << "Output Load for " << gateName << endl;
//         cout << endl;
//         for(double i : G.outputLoad)
//         {
//             cout << i << ", ";
//         }
//         cout << endl;      
//     }
// }

bool isEmptyOrWhitespace(const string& str) {
    return str.empty() || all_of(str.begin(), str.end(), ::isspace);
}

int parsingCircuitFile(string &cktfile, unordered_map<int, cktGates> &netlist) {
    ifstream ifs(cktfile);
    string line;
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
            // cout << "Parsed INPUT: " << nodeID << endl;
        }
        else if (nodeStr.find("OUTPUT") != string::npos) {
            iss >> nodeStr;
            nodeID = stoi(nodeStr);
            netlist[nodeID].isPrimaryOutput = true;
            netlist[nodeID].nodeType = "OUTPUT";
            // cout << "Parsed OUTPUT: " << nodeID << endl;
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

            // cout << "Parsed DFF with input " << dffInputID << " and output " << dffOutputID << endl;
        }
        else {
            nodeID = stoi(nodeStr);
            iss >> nodeStr;
            transform(nodeStr.begin(), nodeStr.end(), nodeStr.begin(), ::toupper);
            netlist[nodeID].nodeType = nodeStr;

            while (iss >> nodeStr) {
                int fanInNode = stoi(nodeStr);
                netlist[nodeID].fanInList.push_back(fanInNode);
                netlist[fanInNode].fanOutList.push_back(nodeID);
            }
        }
    }

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
    unordered_map<int, int> inDegree;
    for(const auto &[nodeID, node] : netlist) {
        inDegree[nodeID] = node.fanInList.size();
    }

    queue<int> q;
    for(const auto &[nodeID, node] : netlist) {
        if(inDegree[nodeID] == 0) {
            q.push(nodeID);
        }
    }

    while(!q.empty()) {
        int node = q.front();
        q.pop();
        sortedList.push_back(node);
        for(int fanOut : netlist.at(node).fanOutList) {
            if(--inDegree[fanOut] == 0) {
                q.push(fanOut);
            }
        }
    }
}

double interpolate(double inputSlew, double loadCap, const gate &gateInfo, bool is_delay) {
    const vector<double> &slewIndices = gateInfo.inputSlew;
    const vector<double> &capIndices = gateInfo.outputLoad;
    
    inputSlew /= 1000.0; // Convert to nanoseconds

    // Find bounding indices for input slew
    auto slewIt = upper_bound(slewIndices.begin(), slewIndices.end(), inputSlew);
    int slewIndex2 = distance(slewIndices.begin(), slewIt);
    int slewIndex1 = max(0, slewIndex2 - 1);
    slewIndex2 = min(static_cast<int>(slewIndices.size()) - 1, slewIndex2);

    // Find bounding indices for load capacitance
    auto capIt = upper_bound(capIndices.begin(), capIndices.end(), loadCap);
    int capIndex2 = distance(capIndices.begin(), capIt);
    int capIndex1 = max(0, capIndex2 - 1);
    capIndex2 = min(static_cast<int>(capIndices.size()) - 1, capIndex2);

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

    // Perform bilinear interpolation
    double t1 = slewIndices[slewIndex1], t2 = slewIndices[slewIndex2];
    double c1 = capIndices[capIndex1], c2 = capIndices[capIndex2];
    double interpolatedValue = 
        (v11 * (c2 - loadCap) * (t2 - inputSlew) +
         v12 * (loadCap - c1) * (t2 - inputSlew) +
         v21 * (c2 - loadCap) * (inputSlew - t1) +
         v22 * (loadCap - c1) * (inputSlew - t1)) /
        ((c2 - c1) * (t2 - t1));

    return interpolatedValue * 1000.0; // Convert back to picoseconds
}

double forwardTraversal(const vector<int> &sortedList, unordered_map<int, cktGates> &netlist, const unordered_map<string, gate> &gates) {
    double circuitDelay = 0;
    for (int node : sortedList) {
        cktGates& currentNode = netlist[node];
        
        if (currentNode.isPrimaryInput || (currentNode.nodeType == "OUTPUT" && !currentNode.isPrimaryOutput)) {
            currentNode.arrivalTime = 0;
            currentNode.inputSlew = 2;
            continue;
        }

        if (currentNode.nodeType == "INPUT" && !currentNode.isPrimaryInput) {
            circuitDelay = max(circuitDelay, currentNode.arrivalTime);
            continue;
        }

        const gate &gateInfo = gates.at(currentNode.nodeType);
        double maxArrivalTime = 0;
        double maxInputSlew = 0;
        double loadCap = currentNode.isPrimaryOutput ? 4 * gates.at("INV").capacitance : 0;

        for (int fanOutNode : currentNode.fanOutList) {
            loadCap += gates.at(netlist[fanOutNode].nodeType).capacitance;
        }

        for (int fanInNode : currentNode.fanInList) {
            double delay = interpolate(netlist[fanInNode].inputSlew, loadCap, gateInfo, true);
            double outputSlew = interpolate(netlist[fanInNode].inputSlew, loadCap, gateInfo, false);
            
            if (currentNode.fanInList.size() > 2) {
                delay *= (currentNode.fanInList.size() / 2.0);
            }

            double arrivalTime = netlist[fanInNode].arrivalTime + delay;
            maxArrivalTime = max(maxArrivalTime, arrivalTime);
            maxInputSlew = max(maxInputSlew, outputSlew);
        }

        currentNode.arrivalTime = maxArrivalTime;
        currentNode.inputSlew = maxInputSlew;

        if (currentNode.isPrimaryOutput) {
            circuitDelay = max(circuitDelay, currentNode.arrivalTime);
        }
    }
    return circuitDelay;
}

void backwardTraversal(const vector<int>& sortedList, unordered_map<int, cktGates>& netlist, const unordered_map<string, gate>& gates, double circuitDelay) {
    double requiredTimeVal = 1.1 * circuitDelay; 

    for (auto& [nodeID, nodeData] : netlist) {
        if (nodeData.isPrimaryOutput) {
            nodeData.requiredArrivalTime = requiredTimeVal;
        } else {
            nodeData.requiredArrivalTime = static_cast<double>(INT_MAX); 
        }
    }

    // Step 2: Traverse the sorted list in reverse to propagate required times
    for (auto it = sortedList.rbegin(); it != sortedList.rend(); ++it) {
        int node = *it;
        cktGates& currentNode = netlist[node];

        // Skip primary output nodes as they're already initialized
        if (currentNode.isPrimaryOutput) {
            currentNode.slack = currentNode.requiredArrivalTime - currentNode.arrivalTime;
            continue;
        }

        double minRequiredTime = static_cast<double>(INT_MAX);

        // Step 3: Calculate required time based on fan-out nodes
        for (int fanOutNode : currentNode.fanOutList) {
            const cktGates& fanOutGate = netlist[fanOutNode];
            const gate& fanOutGateInfo = gates.at(fanOutGate.nodeType);

            // Calculate load capacitance
            double loadCap = fanOutGate.isPrimaryOutput ? 4 * gates.at("INV").capacitance : 0;
            for (int nextFanOutNode : fanOutGate.fanOutList) {
                loadCap += gates.at(netlist[nextFanOutNode].nodeType).capacitance;
            }

            // Calculate delay and required time for the current fan-out connection
            double delay = interpolate(currentNode.inputSlew, loadCap, fanOutGateInfo, true);
            double requiredTime = fanOutGate.requiredArrivalTime - delay;

            minRequiredTime = min(minRequiredTime, requiredTime);
        }

        // Step 4: Set required time and calculate slack
        if (minRequiredTime < static_cast<double>(INT_MAX)) {
            currentNode.requiredArrivalTime = minRequiredTime;
        } else {
            currentNode.requiredArrivalTime = requiredTimeVal;
        }

        // Special handling for primary inputs:
        if (currentNode.isPrimaryInput) {
            double slackForInput = static_cast<double>(INT_MAX);

            // Calculate slack as the required time - delay from fan-out nodes back to input
            for (int fanOutNode : currentNode.fanOutList) {
                const cktGates& fanOutGate = netlist[fanOutNode];
                const gate& fanOutGateInfo = gates.at(fanOutGate.nodeType);
                
                double loadCap = fanOutGate.isPrimaryOutput ? 4 * gates.at("INV").capacitance : 0;
                for (int nextFanOutNode : fanOutGate.fanOutList) {
                    loadCap += gates.at(netlist[nextFanOutNode].nodeType).capacitance;
                }
                
                double delay = interpolate(currentNode.inputSlew, loadCap, fanOutGateInfo, true);
                double requiredTimeForInput = fanOutGate.requiredArrivalTime - delay;
                slackForInput = min(slackForInput, requiredTimeForInput - currentNode.arrivalTime);
            }
            currentNode.slack = slackForInput;
        } else {
            currentNode.slack = currentNode.requiredArrivalTime - currentNode.arrivalTime;
        }
    }
}


vector<int> CriticalPath(const unordered_map<int, cktGates> &netlist) {
    vector<int> criticalPath;
    int currentNode = -1;
    double maxDelay = 0;

    for (const auto &[nodeID, node] : netlist) {
        if (node.isPrimaryOutput && node.arrivalTime > maxDelay) {
            maxDelay = node.arrivalTime;
            currentNode = nodeID;
        }
    }

    while (currentNode != -1) {
        criticalPath.push_back(currentNode);
        if (netlist.at(currentNode).isPrimaryInput) break;

        int prevNode = -1;
        double maxPrevArrivalTime = -1;
        for (int fanInNode : netlist.at(currentNode).fanInList) {
            if (netlist.at(fanInNode).arrivalTime > maxPrevArrivalTime) {
                maxPrevArrivalTime = netlist.at(fanInNode).arrivalTime;
                prevNode = fanInNode;
            }
        }
        currentNode = prevNode;
    }

    reverse(criticalPath.begin(), criticalPath.end());
    return criticalPath;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <library_file> <circuit_file>" << endl;
        return 1;
    }
    // printingData(gates, "XOR"); 
    // printingData(gates, "AND"); 
    // printingData(gates, "NAND"); 
    unordered_map<string, gate> gates;
    library(argv[1], gates);

    unordered_map<int, cktGates> netlist;
    string cktfile = argv[2];
    parsingCircuitFile(cktfile, netlist);

    vector<int> sortedList;
    topologicalSort(netlist, sortedList);

    // unordered_map<int, vector<int>> adjacencyList;
    // adjacencyList = circuitGraph(netlist);

    // std::cout << "Adjacency List (Node -> Immediate Fan-Out Nodes):" << std::endl;
    // for (const auto& [node, fanOutNodes] : adjacencyList) {
    //     std::cout << "Node " << node << " -> ";
    //     for (int fanOutNode : fanOutNodes) {
    //         std::cout << fanOutNode << " ";
    //     }
    //     std::cout << std::endl;
    // }

        // cout << "Topological Order: ";
    // for (int node : sortedList) {
    //     cout << node << " ";
    // }
    // cout << endl;

    double circuitDelay = forwardTraversal(sortedList, netlist, gates);
    backwardTraversal(sortedList, netlist, gates, circuitDelay);

    vector<int> criticalPath = CriticalPath(netlist);

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
    return 0;
}