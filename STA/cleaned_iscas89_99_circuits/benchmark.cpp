#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// Method 1: Line by line reading
void parseLineByLine(const std::string& filename) {
    std::ifstream file(filename);
    std::string line;
    
    // Simulate actual parsing work
    std::vector<std::string> lines;
    while (std::getline(file, line)) {
        // Remove comments
        size_t commentPos = line.find("//");
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }
        
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    
    // Process collected lines
    for (const auto& l : lines) {
        // Simulate token processing
        std::stringstream ss(l);
        std::string token;
        while (ss >> token) {
            // Do something with token
            volatile auto len = token.length(); // Prevent optimization
            (void)len;
        }
    }
}

// Method 2: Reading entire file with rdbuf
void parseWithRdbuf(const std::string& filename) {
    std::ifstream file(filename);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    // Simulate actual parsing work
    std::vector<std::string> lines;
    size_t pos = 0;
    size_t newline;
    
    while ((newline = content.find('\n', pos)) != std::string::npos) {
        std::string line = content.substr(pos, newline - pos);
        
        // Remove comments
        size_t commentPos = line.find("//");
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }
        
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        if (!line.empty()) {
            lines.push_back(line);
        }
        
        pos = newline + 1;
    }
    
    // Process collected lines (same as above)
    for (const auto& l : lines) {
        std::stringstream ss(l);
        std::string token;
        while (ss >> token) {
            volatile auto len = token.length();
            (void)len;
        }
    }
}

// Method 3: Custom memory scan parser (most efficient)
void parseWithCustomScanner(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<char> buffer(size);
    file.read(buffer.data(), size);
    
    // Now implement a custom scanner that works directly with the buffer
    std::vector<std::string> tokens;
    const char* current = buffer.data();
    const char* end = buffer.data() + size;
    
    while (current < end) {
        // Skip whitespace
        while (current < end && (*current == ' ' || *current == '\t' || *current == '\n' || *current == '\r')) {
            current++;
        }
        
        // Check for comments
        if (current + 1 < end && *current == '/' && *(current + 1) == '/') {
            // Skip to end of line
            while (current < end && *current != '\n') {
                current++;
            }
            continue;
        }
        
        // Extract token
        if (current < end) {
            const char* tokenStart = current;
            
            // Find end of token
            while (current < end && *current != ' ' && *current != '\t' && *current != '\n' && *current != '\r') {
                current++;
            }
            
            if (current > tokenStart) {
                tokens.push_back(std::string(tokenStart, current - tokenStart));
            }
        }
    }
    
    // Process tokens
    for (const auto& token : tokens) {
        volatile auto len = token.length();
        (void)len;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;
        return 1;
    }
    
    const std::string filename = argv[1];
    
    // Run each method multiple times to get stable measurements
    const int numRuns = 5;
    
    // Benchmark Method 1
    auto start1 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < numRuns; i++) {
        parseLineByLine(filename);
    }
    auto end1 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed1 = end1 - start1;
    
    // Benchmark Method 2
    auto start2 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < numRuns; i++) {
        parseWithRdbuf(filename);
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed2 = end2 - start2;
    
    // Benchmark Method 3
    auto start3 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < numRuns; i++) {
        parseWithCustomScanner(filename);
    }
    auto end3 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed3 = end3 - start3;
    
    // Report results
    std::cout << "Benchmarking results for " << filename << " (average of " << numRuns << " runs):" << std::endl;
    std::cout << "Line-by-line:   " << elapsed1.count() / numRuns << " seconds" << std::endl;
    std::cout << "rdbuf method:   " << elapsed2.count() / numRuns << " seconds" << std::endl;
    std::cout << "Custom scanner: " << elapsed3.count() / numRuns << " seconds" << std::endl;
    
    return 0;
}