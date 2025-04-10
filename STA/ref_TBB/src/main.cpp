#include <iostream>
#include <vector>
#include <list>
#include <queue>
#include <algorithm>
#include <limits>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>

#include "GateDatabase.hpp"
#include "Circuit.hpp"
#include "TraversalEngine.hpp"

using namespace std;

bool debug = false;

void outputCircuitTraversal (Circuit &circuit, double total_delay, const vector <CircuitNode*> &criticalPath, string outputFile, bool printToTerminal, bool printToFile) {
    ofstream fileOut;
    if (printToFile) { 
        fileOut.open(outputFile);
        if (!fileOut.is_open()) {
            cout << "ERROR: Unable to open file: " << outputFile << endl;
            return;
        }
    }

    ostringstream output;

    output << fixed << setprecision(2) << "Circuit delay: " << total_delay * 1000 << " ps" << endl;
    output << endl;
    output << "Gate slacks:" << endl;

    for (const auto& node : circuit.get_nodes_vector()) {
        if (node != nullptr) {
            std::string type_str = node->get_gate_type();
            if (node->is_input_pad()) {
                type_str = "INP";
            } else if (type_str.empty() && node->is_output_pad()) {
                 type_str = "OUT";
            } else if (type_str.empty()) {
                 type_str = "UNK";
            }

            output << type_str << "-n" << node->get_node_id() << ": " << 1000 * node->gateSlack << " ps" << endl;
        }
    }

    output << endl;
    output << "Critical path:" << endl;

    if (criticalPath.empty()) {
        output << "(No critical path found)" << endl;
    } else {
        for (size_t i = 0; i < criticalPath.size(); ++i) {
            const auto& node = criticalPath[i];
             std::string type_str = node->get_gate_type();
             if (node->is_input_pad()) {
                 type_str = "INP";
             } else if (type_str.empty() && node->is_output_pad()) {
                 type_str = "OUT";
             } else if (type_str.empty()) {
                 type_str = "UNK";
             }
             output << type_str << "-n" << node->get_node_id();
            if (i < criticalPath.size() - 1) {
            output << ", ";
        } 
        }
        output << endl;
    }

    if (printToTerminal) {
     cout << output.str();       
    }

    if (printToFile) {
        fileOut << output.str();
        fileOut.close();        
    }
}

int main(int argc, char* argv[]) {

    // Use command-line arguments
    if (argc < 3) {
        cout << "Error: Not Enough Arguments, Requires 2 Arguments, <library_file> <circuit_file>" << endl;
        return -1;
    }

    // Remove HARDCODED paths 
    // std::string libraryFile = "/home/nishant/EDA/STA/ref/test/NLDM_lib_max2Inp";
    // std::string circuitFile = "/home/nishant/EDA/STA/ref/test/cleaned_iscas89_99_circuits/c17.isc";

    string libraryFile = argv[1]; // Use argument
    string circuitFile = argv[2]; // Use argument

    cout << "Using Library File: " << libraryFile << endl;
    cout << "Using Circuit File: " << circuitFile << endl;

    cout << "Initializing circuit..." << endl;
    Circuit circuit (circuitFile, libraryFile);
    cout << "Circuit initialization complete." << endl;

    cout << "Running parallel traversals..." << endl;
    TraversalEngine engine(circuit);
    
    engine.run_parallel_forward_traversal();
    double total_delay = engine.get_total_circuit_delay();
    
    double required_time_target = total_delay * 1.1; 
    engine.run_parallel_backward_traversal(required_time_target);
    
    vector<CircuitNode*> criticalPath = engine.find_critical_path();
    cout << "Traversals complete." << endl;

    cout << "Generating output..." << endl;
    outputCircuitTraversal(circuit, total_delay, criticalPath, "ckt_traversal.txt", true, false);
    cout << "Output complete." << endl;

    return 0;
}
