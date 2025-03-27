// tokenscanner.hpp
#ifndef TOKEN_SCANNER_HPP
#define TOKEN_SCANNER_HPP

#include <string>
#include <fstream>
#include <vector>
#include <cctype>
#include <cstring>
#include "debug.hpp"

class TokenScanner {
private:
    std::vector<char> buffer_;
    const char* current_;
    const char* end_;
    
public:
    // Constructor that loads file directly
    explicit TokenScanner(const std::string& filename)
    : current_(nullptr), end_(nullptr)
{
    // Temporarily comment out:
     loadFile(filename);
    Debug::trace("TokenScanner constructed for " + filename + " (loadFile SKIPPED)");
}
    
    // Constructor that uses an existing buffer
    TokenScanner(const char* buffer, size_t size) 
        : buffer_(buffer, buffer + size), 
          current_(buffer_.data()), 
          end_(buffer_.data() + size) {}
    
          bool loadFile(const std::string& filename) {
            Debug::trace("loadFile: Entered for " + filename);
            try {
                Debug::trace("loadFile: Creating ifstream...");
                std::ifstream file(filename, std::ios::binary | std::ios::ate);
                Debug::trace("loadFile: ifstream object created.");
        
                Debug::trace("loadFile: Checking if file is open...");
                if (!file.is_open()) {
                    // Use Debug::error here as Debug framework should be initialized by now
                    Debug::error("TokenScanner::loadFile FAILED to open: " + filename);
                    return false;
                }
                Debug::trace("loadFile: File is open.");
        
                Debug::trace("loadFile: Getting file size (tellg)...");
                std::streamsize size = file.tellg();
                Debug::trace("loadFile: File size = " + std::to_string(size));
        
                Debug::trace("loadFile: Seeking to beginning (seekg)...");
                file.seekg(0, std::ios::beg);
                Debug::trace("loadFile: Seek complete.");
        
                // Handle potential empty file case
                if (size <= 0) {
                     Debug::warn("TokenScanner::loadFile: File is empty or size is non-positive for " + filename);
                     buffer_.clear(); // Ensure buffer is empty
                     current_ = nullptr;
                     end_ = nullptr;
                     return true; // Technically successful loading of an empty file
                }
        
        
                Debug::trace("loadFile: Resizing buffer...");
                buffer_.resize(size);
                Debug::trace("loadFile: Buffer resized.");
        
                Debug::trace("loadFile: Reading file content...");
                file.read(buffer_.data(), size);
                Debug::trace("loadFile: File read complete.");
        
                // Check stream state after read (optional but good)
                if (!file) {
                     Debug::warn("TokenScanner::loadFile: Stream state indicates error after reading " + std::to_string(file.gcount()) + " bytes from " + filename);
                     // Decide if this is a fatal error or not
                }
        
        
                current_ = buffer_.data();
                end_ = buffer_.data() + size;
                Debug::trace("loadFile: Buffer pointers set.");
        
            } catch (const std::bad_cast& bc) {
                // Catch bad_cast specifically
                Debug::error("loadFile: Caught std::bad_cast during file operations for " + filename + ": " + bc.what());
                return false;
            } catch (const std::exception& e) {
                // Catch other potential standard exceptions (e.g., from resize)
                Debug::error("loadFile: Caught std::exception during file operations for " + filename + ": " + e.what());
                return false;
            }
        
            Debug::trace("loadFile: Exiting successfully for " + filename);
            return true;
        }
            
    bool hasMoreTokens() const {
        return current_ < end_;
    }
    
    // Skip whitespace and comments (Handles // and /* */)
    void skipWhitespaceAndComments() {
        while (current_ < end_) {
            // Skip whitespace (space, tab, newline, carriage return, etc.)
            if (isspace(*current_)) {
                current_++;
                continue; // Check next character
            }

            // Skip C++ style comments (// style)
            if (current_ + 1 < end_ && *current_ == '/' && *(current_ + 1) == '/') {
                // Advance until newline or end of buffer
                while (current_ < end_ && *current_ != '\n') {
                    current_++;
                }
                // If we found a newline, advance past it
                if (current_ < end_ && *current_ == '\n') {
                    current_++;
                }
                continue; // Check character after the comment line
            }

            // Skip C style comments (/* ... */)
            if (current_ + 1 < end_ && *current_ == '/' && *(current_ + 1) == '*') {
                current_ += 2; // Move past /*
                // Advance until */ is found or end of buffer
                while (current_ + 1 < end_) {
                    if (*current_ == '*' && *(current_ + 1) == '/') {
                        current_ += 2; // Move past */
                        goto next_iteration; // Use goto to jump to the outer loop's next iteration check
                    }
                    current_++; // Move to next character inside the comment
                }
                // Reached end of buffer without finding */ (unterminated comment)
                // The loop condition (current_ < end_) will handle exiting.
                // We jump here if the inner loop finishes by reaching end_.
                next_iteration:;
                continue; // Check character after the */ or after reaching end_ in comment
            }

            // If none of the above, it's not whitespace or a comment start
            break; // Exit the while loop, current_ points to a valid token start
        }
    }

    // --- Keep the rest of the TokenScanner class members ---
    // loadFile, hasMoreTokens, nextToken, getLine, peekToken, consumeIf, getLineNumber
    
    // Get next token
    std::string nextToken() {
        skipWhitespaceAndComments();
        
        if (current_ >= end_)
            return "";
            
        const char* tokenStart = current_;
        
        // Handle different token types
        if (isalpha(*current_) || *current_ == '_') {
            // Identifier - includes [0] array notation
            while (current_ < end_ && 
                  (isalnum(*current_) || *current_ == '_' || 
                   *current_ == '[' || *current_ == ']')) {
                current_++;
            }
        }
        else if (isdigit(*current_) || 
                (*current_ == '.' && current_+1 < end_ && isdigit(*(current_+1)))) {
            // Number (integer or floating point)
            bool hasDecimal = false;
            while (current_ < end_ && 
                   (isdigit(*current_) || (*current_ == '.' && !hasDecimal))) {
                if (*current_ == '.')
                    hasDecimal = true;
                current_++;
            }
        }
        else if (*current_ == '(' || *current_ == ')' || *current_ == '{' || 
                *current_ == '}' || *current_ == ',' || *current_ == '=' || 
                *current_ == ';' || *current_ == ':') {
            // Special character
            current_++;
        }
        else {
            // Unknown character, just advance
            current_++;
        }
        
        return std::string(tokenStart, current_ - tokenStart);
    }
    
    // Get a complete line
    std::string getLine() {
            skipWhitespaceAndComments();
    
    if (current_ >= end_)
        return "";
        
    // Remember starting position before we skip any character
    const char* lineStart = current_;
    
    // Advance to end of line
    while (current_ < end_ && *current_ != '\n')
        current_++;
        
    // Create the line string
    std::string line(lineStart, current_ - lineStart);
    
    // Move past newline if present
    if (current_ < end_ && *current_ == '\n')
        current_++;
        
    // Trim trailing whitespace but NOT leading whitespace
    // (leading numbers are important for gate IDs)
    size_t endPos = line.find_last_not_of(" \t\r");
    if (endPos != std::string::npos)
        line = line.substr(0, endPos + 1);
    
    return line;
}
   
    
    // Peek at the next token without consuming it
    std::string peekToken() {
        const char* savedPos = current_;
        std::string token = nextToken();
        current_ = savedPos;
        return token;
    }
    
    // Check if current position matches the given token and consume it if true
// In tokenscanner.hpp
bool consumeIf(const std::string& expected) {
    skipWhitespaceAndComments();
    size_t len = expected.length();

    if (current_ + len <= end_ &&
        strncmp(current_, expected.c_str(), len) == 0) {

        // --- MODIFIED BOUNDARY CHECK ---
        // Check if it's a complete token match OR
        // if it's a single non-alphanumeric character (like '(', '{', ';', etc.)
        bool isSingleSymbol = (len == 1 && !isalnum(*current_));

        if (isSingleSymbol || current_ + len == end_ || !isalnum(*(current_ + len))) {
        // --- END MODIFIED BOUNDARY CHECK ---
            current_ += len;
            return true;
        }
        // If the boundary check fails (e.g. consuming "lib" when next char is 'r')
        // Debug::trace("consumeIf failed boundary check for '" + expected + "'"); // Optional trace
    }
    return false;
}
     
    // Get current line number for error reporting
    int getLineNumber() const {
        int line = 1;
        for (const char* p = buffer_.data(); p < current_; p++) {
            if (*p == '\n') line++;
        }
        return line;
    }
};

#endif // TOKEN_SCANNER_HPP