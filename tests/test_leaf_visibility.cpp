#include "../sampler.h"
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

namespace {

volatile double sink = 0;

#define BARRIER_LOOP(n)                                                        \
    do {                                                                       \
        double a = 0;                                                          \
        for (long i = 0; i < (n); ++i) {                                       \
            a += i * 0.5;                                                      \
            asm volatile("" : "+w"(a));                                        \
        }                                                                      \
        sink = a;                                                              \
    } while (0)

// Makes no calls -> no frame -> invisible to frame-pointer unwinding.
__attribute__((noinline)) void leaf_only(long n) { BARRIER_LOOP(n); }
__attribute__((noinline)) void calibration_loop(long n) { BARRIER_LOOP(n); }

__attribute__((noinline)) void leaf_caller(long n) {
    leaf_only(n);
    asm volatile("" ::: "memory");   // keep this from becoming a tail call
}

long iterations_per_second() {
    const long probe = 20000000;
    auto start = std::chrono::steady_clock::now();
    calibration_loop(probe);
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                  std::chrono::steady_clock::now() - start).count();
    if (us < 1000) us = 1000;
    return static_cast<long>(probe * 1000000.0 / static_cast<double>(us));
}

} // namespace

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::cerr << "test_leaf_visibility FAIL: " << msg << "\n";        \
            return 1;                                                          \
        }                                                                      \
    } while (0)

int main() {
    const std::string outputPath = "tests/leaf_visibility_output.txt";
    std::remove(outputPath.c_str());

    const long unit = iterations_per_second();

    {
        Sampler sampler(outputPath, 200);
        leaf_caller(2 * unit);
    }

    int lines = 0, leafSamples = 0, callerPresent = 0;
    {
        std::ifstream in(outputPath);
        std::string line;
        while (std::getline(in, line)) {
            ++lines;
            const std::string self = line.substr(line.find_last_of(';') + 1);
            if (self.find("leaf_only") != std::string::npos) ++leafSamples;
            if (line.find("leaf_caller") != std::string::npos) ++callerPresent;
        }
    }
    std::remove(outputPath.c_str());

    std::cout << "samples: " << lines
              << "  leaf_only as leaf: " << leafSamples
              << "  leaf_caller in stack: " << callerPresent << "\n";

    CHECK(lines >= 100, "too few samples (" << lines << ")");
    CHECK(callerPresent > 0, "leaf_caller() missing -- frame chain is broken");
    CHECK(leafSamples * 2 > lines,
          "leaf_only() is the leaf in only " << leafSamples << " of " << lines
          << " samples; the interrupted PC is not being read from ucontext");

    std::cout << "test_leaf_visibility: PASS\n";
    return 0;
}
