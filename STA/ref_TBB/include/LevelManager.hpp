#ifndef LEVELMANAGER_HPP
#define LEVELMANAGER_HPP

#include <vector>
#include <map>
#include "CircuitNode.hpp" // Assuming CircuitNode definition is here

class Circuit; // Forward declaration

class LevelManager {
public:
    LevelManager() = default;

    // Computes forward (inputs to outputs) and reverse (outputs to inputs) levels
    void compute_levels(Circuit& circuit);

    // Accessors for the computed levels
    const std::vector<std::vector<CircuitNode*>>& get_forward_levels() const;
    const std::vector<std::vector<CircuitNode*>>& get_reverse_levels() const;
    size_t get_max_level() const;


private:
    void compute_forward_levels(Circuit& circuit);
    void compute_reverse_levels(Circuit& circuit);

    std::vector<std::vector<CircuitNode*>> forward_levels_;
    std::vector<std::vector<CircuitNode*>> reverse_levels_;
    size_t max_level_ = 0;
};

#endif // LEVELMANAGER_HPP 