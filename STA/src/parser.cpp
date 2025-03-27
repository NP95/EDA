// parser.cpp
#include "parser.hpp"
#include <sys/stat.h>
#include "debug.hpp" // Make sure debug is included


bool Parser::shouldUseScanner() const {
    // Decide based on file size - use scanner for files > 1MB
    struct stat statbuf;
    if (stat(filename_.c_str(), &statbuf) == 0) {
        return statbuf.st_size > 1 * 1024 * 1024; // 1MB threshold
    }
    return true; // Default to using scanner if we can't check file size
}

bool Parser::initialize() {
    Debug::trace("Entering Parser::initialize() for " + filename_ + " with useScanner_ = " + (useScanner_ ? "true" : "false")); // Use Debug::trace
    if (useScanner_) {
        Debug::trace("Trying to create TokenScanner..."); // Use Debug::trace
        try {
            scanner_ = std::make_unique<TokenScanner>(filename_);
            if (!scanner_) {
                 Debug::error("make_unique<TokenScanner> resulted in nullptr for " + filename_); // Use Debug::error
                 return false;
            }
            Debug::trace("TokenScanner object created (pointer is not null)."); // Use Debug::trace

            // Optional: Add a method to TokenScanner like 'bool isReady() const'
            // that returns true only if loadFile succeeded.
            // if (!scanner_->isReady()) {
            //     Debug::error("TokenScanner created but failed to load file " + filename_);
            //     return false;
            // }

        } catch (const std::exception& e) {
             Debug::error("Exception creating TokenScanner for " + filename_ + ": " + e.what()); // Use Debug::error
             return false;
        }
        Debug::trace("Exiting Parser::initialize() successfully (scanner path)."); // Use Debug::trace
        return true;
    } else {
        Debug::trace("Using ifstream path..."); // Use Debug::trace
        bool fileOk = openFile();
        // Use Debug::error if openFile fails internally
        // if (!fileOk) Debug::error("openFile() failed for " + filename_);
        Debug::trace(std::string("Exiting Parser::initialize() (ifstream path) with status: ") + (fileOk ? "true" : "false"));
        return fileOk;
    }
}

bool Parser::openFile() {
    file_.open(filename_);
    if (!file_.is_open()) {
        std::cerr << "Error: Could not open file " << filename_ << std::endl;
        return false;
    }
    return true;
}

std::string Parser::getLine() {
    if (scanner_) {
        return scanner_->getLine();
    }
    
    std::string line;
    while (std::getline(file_, line)) {
        // Remove comments
        size_t commentPos = line.find("//");
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }
        
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        if (!line.empty()) {
            return line;
        }
    }
    return "";
}

std::vector<std::string> Parser::tokenize(const std::string& line, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(line);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        token.erase(0, token.find_first_not_of(" \t"));
        token.erase(token.find_last_not_of(" \t") + 1);
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}