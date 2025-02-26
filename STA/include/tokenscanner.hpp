// tokenscanner.hpp
#ifndef TOKEN_SCANNER_HPP
#define TOKEN_SCANNER_HPP

#include <string>
#include <fstream>
#include <vector>
#include <cctype>
#include <cstring>

class TokenScanner {
private:
    std::vector<char> buffer_;
    const char* current_;
    const char* end_;
    
public:
    // Constructor that loads file directly
    explicit TokenScanner(const std::string& filename) {
        loadFile(filename);
    }
    
    // Constructor that uses an existing buffer
    TokenScanner(const char* buffer, size_t size) 
        : buffer_(buffer, buffer + size), 
          current_(buffer_.data()), 
          end_(buffer_.data() + size) {}
    
    bool loadFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return false;
        
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        buffer_.resize(size);
        file.read(buffer_.data(), size);
        
        current_ = buffer_.data();
        end_ = buffer_.data() + size;
        return true;
    }
    
    bool hasMoreTokens() const {
        return current_ < end_;
    }
    
    // Skip whitespace and comments
    void skipWhitespaceAndComments() {
        while (current_ < end_) {
            // Skip whitespace
            if (isspace(*current_)) {
                current_++;
                continue;
            }
            
            // Skip comments (// style)
            if (current_ + 1 < end_ && *current_ == '/' && *(current_ + 1) == '/') {
                while (current_ < end_ && *current_ != '\n')
                    current_++;
                continue;
            }
            
            break;  // Not whitespace or comment
        }
    }
    
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
            
        const char* lineStart = current_;
        
        // Advance to end of line
        while (current_ < end_ && *current_ != '\n')
            current_++;
            
        // Move past newline if present
        if (current_ < end_ && *current_ == '\n')
            current_++;
            
        return std::string(lineStart, current_ - lineStart);
    }
    
    // Peek at the next token without consuming it
    std::string peekToken() {
        const char* savedPos = current_;
        std::string token = nextToken();
        current_ = savedPos;
        return token;
    }
    
    // Check if current position matches the given token and consume it if true
    bool consumeIf(const std::string& expected) {
        skipWhitespaceAndComments();
        size_t len = expected.length();
        
        if (current_ + len <= end_ && 
            strncmp(current_, expected.c_str(), len) == 0) {
            // Make sure this is a complete token
            if (current_ + len == end_ || 
                !isalnum(*(current_ + len))) {
                current_ += len;
                return true;
            }
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