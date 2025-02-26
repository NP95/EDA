// parser.cpp
#include "parser.hpp"
#include <sys/stat.h>

bool Parser::shouldUseScanner() const {
    // Decide based on file size - use scanner for files > 1MB
    struct stat statbuf;
    if (stat(filename_.c_str(), &statbuf) == 0) {
        return statbuf.st_size > 1 * 1024 * 1024; // 1MB threshold
    }
    return true; // Default to using scanner if we can't check file size
}

bool Parser::initialize() {
    if (useScanner_ && shouldUseScanner()) {
        scanner_ = std::make_unique<TokenScanner>(filename_);
        return scanner_ != nullptr;
    } else {
        return openFile();
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