#include "../sampler.h"
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

// 3:2:1 test, func_a should be ~50.0%, func_b should be ~33.3%, and func_c should be 16.7%

namespace {

volatile long sink = 0;

__attribute__((noinline)) void func_a() {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    long acc = 0;
    while (std::chrono::steady_clock::now() < deadline) { for (int i = 0; i < 100000; ++i) acc += i; }
    sink = acc;
}
__attribute__((noinline)) void func_b() {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    long acc = 0;
    while (std::chrono::steady_clock::now() < deadline) { for (int i = 0; i < 100000; ++i) acc += i; }
    sink = acc;
}
__attribute__((noinline)) void func_c() {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    long acc = 0;
    while (std::chrono::steady_clock::now() < deadline) { for (int i = 0; i < 100000; ++i) acc += i; }
    sink = acc;
}

// Counts self samples per function
std::map<std::string, int> count_self_samples(const std::string& path) {
    std::map<std::string, int> counts;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        std::string self = line.substr(line.find_last_of(';') + 1);
        for (const char* name : {"func_a", "func_b", "func_c"}) {
            if (self.find(name) != std::string::npos) {
                counts[name]++;
                break;
            }
        }
    }
    return counts;
}

} // namespace

int main() {
    const std::string outputPath = "tests/sampler_accuracy_output.txt";
    std::remove(outputPath.c_str());

    {
        Sampler sampler(outputPath, 200);
        func_a();
        func_b();
        func_c();
    }

    auto counts = count_self_samples(outputPath);
    std::remove(outputPath.c_str());

    int total = counts["func_a"] + counts["func_b"] + counts["func_c"];
    assert(total > 0 && "no samples landed in any spin function");

    double pct_a = 100.0 * counts["func_a"] / total;
    double pct_b = 100.0 * counts["func_b"] / total;
    double pct_c = 100.0 * counts["func_c"] / total;

    std::cout << "func_a: " << counts["func_a"] << " samples (" << pct_a << "%)\n"
              << "func_b: " << counts["func_b"] << " samples (" << pct_b << "%)\n"
              << "func_c: " << counts["func_c"] << " samples (" << pct_c << "%)\n";


    const double tolerance = 4.0;
    assert(std::fabs(pct_a - 50.0) < tolerance && "func_a share off");
    assert(std::fabs(pct_b - 33.3) < tolerance && "func_b share off");
    assert(std::fabs(pct_c - 16.7) < tolerance && "func_c share off");

    std::cout << "test_sampler_accuracy: PASS\n";
    return 0;
}
