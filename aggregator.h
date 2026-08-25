#pragma once

#include <string>
#include <unordered_map>

class Aggregator {
private:
    std::string fileName_;
    std::unordered_map<std::string, std::uint64_t> selfCounts_;
    std::unordered_map<std::string, std::uint64_t> totalCounts_;
public:
    // Constructor
    Aggregator(std::string fileName);

    // Process each line of a given file
    void processFile();

    // Format results and print to console
    void output() const;
};

