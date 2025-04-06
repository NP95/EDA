#include "GateLibrary.hpp"
#include "Debug.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>

// Utility function to check if two values are approximately equal
bool approximatelyEqual(double a, double b) {
    double diff = std::fabs(a - b);
    double percentage = diff / std::max(std::fabs(a), std::fabs(b)) * 100.0;
    return percentage <= 12.0; // Use 12% relative difference as tolerance
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <liberty_file>" << std::endl;
        return 1;
    }

    // Set debug level
    DebugLogger::getInstance().setLevel(DebugLevel::TRACE);
    std::cout << "=== Validating Interpolation Functions ===" << std::endl;

    // Parse the liberty file
    GateLibrary gateLibrary;
    if (!gateLibrary.parseLibertyFile(argv[1])) {
        std::cerr << "Error: Failed to parse liberty file: " << argv[1] << std::endl;
        return 1;
    }

    // Get the NAND gate info from the library
    const Gate* nandGate = gateLibrary.getGate("NAND");
    if (!nandGate) {
        std::cerr << "Error: NAND gate not found in library" << std::endl;
        return 1;
    }

    // Manual trace test cases
    struct TestCase {
        std::string nodeDesc;
        double inputSlew;
        double loadCap;
        double expectedSlew;
        double expectedDelay;
    };

    TestCase testCases[] = {
        {"Node 9 (NAND)", 2.00, 3.198, 9.96, 14.03},  // Node 9 test case
        {"Node 8 (NAND)", 2.00, 1.599, 7.51, 10.79},  // Node 8 test case
        {"Node 10 (NAND)", 9.96, 3.198, 11.37, 19.84} // Node 10 test case
    };

    // Test each case
    bool allPassed = true;
    for (const auto& tc : testCases) {
        double actualSlew = nandGate->getOutputSlew(tc.inputSlew, tc.loadCap);
        double actualDelay = nandGate->getDelay(tc.inputSlew, tc.loadCap);

        // Check if values match with the expected values
        bool slewMatch = approximatelyEqual(actualSlew, tc.expectedSlew);
        bool delayMatch = approximatelyEqual(actualDelay, tc.expectedDelay);

        // Print results
        std::cout << "----------------------------------------" << std::endl;
        std::cout << "Test Case: " << tc.nodeDesc << std::endl;
        std::cout << "  Input: τ=" << tc.inputSlew << " ps, C=" << tc.loadCap << " fF" << std::endl;
        
        double slewDiff = std::fabs(actualSlew - tc.expectedSlew);
        double slewPercent = slewDiff / std::max(std::fabs(actualSlew), std::fabs(tc.expectedSlew)) * 100.0;
        std::cout << "  Slew:  Expected=" << std::fixed << std::setprecision(2) << tc.expectedSlew 
                  << " ps, Actual=" << actualSlew
                  << " ps, Diff=" << slewPercent << "%, Match=" << (slewMatch ? "YES" : "NO") << std::endl;
        
        double delayDiff = std::fabs(actualDelay - tc.expectedDelay);
        double delayPercent = delayDiff / std::max(std::fabs(actualDelay), std::fabs(tc.expectedDelay)) * 100.0;
        std::cout << "  Delay: Expected=" << tc.expectedDelay 
                  << " ps, Actual=" << actualDelay
                  << " ps, Diff=" << delayPercent << "%, Match=" << (delayMatch ? "YES" : "NO") << std::endl;

        if (!slewMatch || !delayMatch) {
            allPassed = false;
        }
    }

    // Summary
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Overall Result: " << (allPassed ? "ALL TESTS PASSED" : "SOME TESTS FAILED") << std::endl;

    return allPassed ? 0 : 1;
} 