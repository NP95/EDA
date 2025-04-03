// Utils.hpp
#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <sstream> // Needed for splitAndTrim implementation detail exposed via template/inline
#include <stdexcept> // Needed for exceptions potentially thrown by helpers
#include <algorithm> // Needed for trim implementation detail

namespace Utils {

    // Trim whitespace from start and end of a string
    // (Implementation can be inline in header for simple functions)
    inline std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\n\r");
        if (std::string::npos == first) {
            return ""; // Return empty string if all whitespace
        }
        size_t last = str.find_last_not_of(" \t\n\r");
        return str.substr(first, (last - first + 1));
    }

    // Split a string by a delimiter, trimming whitespace
    // (Implementation can be inline in header)
    inline std::vector<std::string> splitAndTrim(const std::string& s, char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(s);
        while (std::getline(tokenStream, token, delimiter)) {
            tokens.push_back(Utils::trim(token)); // Use Utils::trim
        }
        return tokens;
    }

     // Safely convert string to double - Declaration
    double stringToDouble(const std::string& s, const std::string& context);

     // Safely convert string to int - Declaration
    int stringToInt(const std::string& s, const std::string& context);

} // namespace Utils

#endif // UTILS_H