#include "aggregator.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>

Aggregator::Aggregator(std::string fileName) : fileName_(std::move(fileName)) {
}

void Aggregator::processFile() {
    std::ifstream file(fileName_);
    if (!file.is_open()) { 
        std::cerr << "Error: Could not open the file!" << std::endl; 
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        functionCounts_[line]++;
    }
}

void Aggregator::output() const {

    std::vector<std::pair<std::string, int>> pairs(functionCounts_.begin(), functionCounts_.end());

    std::sort(pairs.begin(), pairs.end(), [](const auto& a, const auto& b) {
        return a.second > b.second; 
    });

    for (const auto& [key, value] : pairs) {
        std::cout << key << ": " << value << '\n';
    }

}

