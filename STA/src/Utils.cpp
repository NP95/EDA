// Utils.cpp
#include "Utils.hpp"
#include <string>
#include <stdexcept> // For actual exception throwing

namespace Utils {

    // Safely convert string to double - Implementation
    double stringToDouble(const std::string& s, const std::string& context) {
        try {
            // Use stod for conversion
            size_t processed = 0;
            double result = std::stod(s, &processed);
             // Basic check if the whole string was processed (stod might stop early)
             if (processed != s.length()) {
                  // Allow trailing whitespace after number? Let's trim first.
                  std::string trimmed_s = Utils::trim(s);
                  processed = 0;
                  result = std::stod(trimmed_s, &processed);
                  if(processed != trimmed_s.length()){
                       // Still not fully processed, likely invalid chars
                        throw std::invalid_argument("Invalid characters found after number");
                  }
             }

            return result;
        } catch (const std::invalid_argument& ia) {
            throw std::runtime_error("Invalid numeric format in " + context + ": '" + s + "'");
        } catch (const std::out_of_range& oor) {
            throw std::runtime_error("Numeric value out of range in " + context + ": '" + s + "'");
        }
         // Catch potential exception from our own check
         catch(const std::runtime_error& re){
             throw re; // Rethrow
         }
         catch(...){ // Catch any other unexpected issue during conversion
             throw std::runtime_error("Unknown error converting numeric value in " + context + ": '" + s + "'");
         }
    }

    // Safely convert string to int - Implementation
    int stringToInt(const std::string& s, const std::string& context) {
        try {
             size_t processed = 0;
             int result = std::stoi(s, &processed);
             // Basic check if the whole string was processed
             if (processed != s.length()) {
                 std::string trimmed_s = Utils::trim(s);
                 processed = 0;
                 result = std::stoi(trimmed_s, &processed);
                 if(processed != trimmed_s.length()){
                      throw std::invalid_argument("Invalid characters found after integer");
                 }
            }
            return result;
        } catch (const std::invalid_argument& ia) {
            throw std::runtime_error("Invalid integer format in " + context + ": '" + s + "'");
        } catch (const std::out_of_range& oor) {
            throw std::runtime_error("Integer value out of range in " + context + ": '" + s + "'");
        }
        // Catch potential exception from our own check
         catch(const std::runtime_error& re){
             throw re; // Rethrow
         }
        catch(...){
             throw std::runtime_error("Unknown error converting integer value in " + context + ": '" + s + "'");
        }
    }

} // namespace Utils