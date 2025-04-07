#ifndef GATEDATABASE_HPP
#define GATEDATABASE_HPP

#include <string>
#include <vector>
#include <map>

#define GATE_LUT_DIM 7

struct GateInfo {
    double capacitance;
    double cell_delayindex1[GATE_LUT_DIM];
    double cell_delayindex2[GATE_LUT_DIM];
    double output_slewindex1[GATE_LUT_DIM];
    double output_slewindex2[GATE_LUT_DIM];
    double cell_delay[GATE_LUT_DIM][GATE_LUT_DIM];
    double output_slew[GATE_LUT_DIM][GATE_LUT_DIM];
};

class GateDatabase {
    private:
        // Stores the pointer to the GateInfo. Indexed using the name of gate
        // If you want to avoid pointers entirely, you may have to make GateInfo
        // with vectors instead of simple arrays
        //std::map<std::string, GateInfo*> gate_info_lut_;
    public:
        std::map<std::string, GateInfo*> gate_info_lut_;
        GateDatabase(const std::string& file_name);
        ~GateDatabase();

        void insert(const std::string& gateName, GateInfo* gateInfo);
        const GateInfo* get_gate_info(const std::string& gateName);

        void test();
};

#endif //GATEDATABASE_HPP
