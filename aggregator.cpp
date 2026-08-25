#include "aggregator.h"
#include <unordered_set>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string_view>
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
        std::unordered_set<std::string> functions;
        // Create stringstream of the line in the file
        std::stringstream ss(line);
        // Create variable for "function" tokens to be held
        std::string function;
        // Break line up by instances of delimiter
        while (std::getline(ss, function, ';')) {
            auto it = functions.find(function);
            if (it == functions.end()) {
                functions.insert(function);
                totalCounts_[function]++;
            }
        }
        selfCounts_[function]++;
    }
}

void Aggregator::output() const {

    std::unordered_set<std::string> names;
    for (const auto& [key, value] : selfCounts_) names.insert(key);
    for (const auto& [key, value] : totalCounts_) names.insert(key);

    std::vector<std::string> functions(names.begin(), names.end());

    auto selfOf = [this](const std::string& name) {
        auto it = selfCounts_.find(name);
        return it != selfCounts_.end() ? it->second : std::uint64_t{0};
    };
    auto totalOf = [this](const std::string& name) {
        auto it = totalCounts_.find(name);
        return it != totalCounts_.end() ? it->second : std::uint64_t{0};
    };

    std::sort(functions.begin(), functions.end(), [&](const auto& a, const auto& b) {
        return selfOf(a) > selfOf(b);
    });

    for (const auto& name : functions) {
        std::cout << name << ": self=" << selfOf(name) << " total=" << totalOf(name) << '\n';
    }

}

