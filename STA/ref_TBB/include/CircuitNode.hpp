#ifndef CIRCUITNODE_HPP
#define CIRCUITNODE_HPP

#include <string>
#include <list>
#include <vector>
#include "GateDatabase.hpp"

typedef int NodeID;

class CircuitNode {
    private:

    public:

        NodeID node_id_;
        bool input_pad_;
        bool output_pad_;
        std::string gate_type_;
        const GateInfo* gate_info_;
        std::vector<NodeID> fanin_list_;

        // data added from given code
        std::vector<NodeID> fanout_list; //vector of gates connected to this particular gate
        int inDegree; // number of inputs to this particular gate
        int outDegree; // number of gates connected to the output of this particular gate



        std::vector<double> inputArrivalTimes; // input arrival times for each input to the gate
        std::vector<double> outputArrivalTimes; // using the input arrival time vector we determin the ouptput arrival time for each input
        std::vector<double> gateDelays;
        std::vector<double> inputSlews; // input slews for each input to the gate
        std::vector<double> outputSlews; // output slews for each output to the gate
        double outputLoad; // capacitance of all the gates connected to this gates outputs' summed together
        double gateSlack; // use backwards traversal to aquire for each gate
        double slewOut; // actual output slew time from maxamizing the output vector 
        double timeOut; // Actual output time from maximizing the output vector
        double requiredArrivalTime;
        double cellDelay;


        // constructers
        CircuitNode() :
            node_id_(-1), 
            input_pad_(false), 
            output_pad_(false),
            gate_type_(""), 
            gate_info_(nullptr),

            fanin_list_(), 
            fanout_list(),

            inDegree(0),
            outDegree(0),

            inputArrivalTimes(),
            outputArrivalTimes(),
            gateDelays(),
            inputSlews(),
            outputSlews(),

            outputLoad(0),
            gateSlack(0),           
            slewOut(0),
            timeOut(0),

            requiredArrivalTime(0),
            cellDelay(0)
            
            { }
        
        void set_node_id(const NodeID& node_id);
        void set_input_pad(const bool& input_pad);
        void set_output_pad(const bool& output_pad);
        void set_gate_type(const std::string& gate_type);
        void set_gate_info(const GateInfo* gate_info);
        void add_to_fanin_list(const NodeID& node_id);
        void clear_fanin_list();
        void set_time_out(double time);
        void set_slew_out(double slew);
        void set_initial_timing_conditions();
        
        const NodeID& get_node_id() const;
        const bool& is_input_pad() const;
        const bool& is_output_pad() const;
        const std::string& get_gate_type() const;
        const GateInfo* get_gate_info() const;
        const std::vector<NodeID>& get_fanin_list() const;
        const std::vector<NodeID>& get_fanout_list() const { return fanout_list; }

        // Add getters for timing info if needed outside traversal
        double get_time_out() const { return timeOut; }
        double get_slew_out() const { return slewOut; }
        double get_gate_slack() const { return gateSlack; }

        // --- Initialization & Calculation Methods ---
        // Initialize vectors based on fanin count
        void initialize_timing_vectors();

        // Placeholder for forward calculation logic (to be called by TraversalEngine)
        // Pass gate_db for access to TimingUtils
        void calculate_output_timing(const GateDatabase& gate_db, const std::vector<CircuitNode*>& circuit_nodes);

        // Placeholder for backward calculation logic (to be called by TraversalEngine)
        // Pass gate_db for access to TimingUtils if needed (though maybe not required here)
        void calculate_required_time_and_slack(const GateDatabase& gate_db, const std::vector<CircuitNode*>& circuit_nodes, double circuit_max_delay);
};

#endif //CIRCUITNODE_HPP
