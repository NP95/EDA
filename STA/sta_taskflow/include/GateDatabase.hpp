#ifndef GATEDATABASE_HPP
#define GATEDATABASE_HPP

#include <string>
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
    std::map<std::string, GateInfo*> gate_info_lut_;
    
public:
    GateDatabase(const std::string& file_name);
    ~GateDatabase();
    
    void insert(const std::string& gateName, GateInfo* gateInfo);
    const GateInfo* get_gate_info(const std::string& gateName) const;
    const std::map<std::string, GateInfo*>& get_gate_info_lut() const { return gate_info_lut_; }
};

#endif 