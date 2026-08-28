#include "../sampler.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
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

__attribute__((noinline)) void func_a(long n) { BARRIER_LOOP(n); }
__attribute__((noinline)) void func_b(long n) { BARRIER_LOOP(n); }
__attribute__((noinline)) void func_c(long n) { BARRIER_LOOP(n); }
__attribute__((noinline)) void calibration_loop(long n) { BARRIER_LOOP(n); }

long iterations_per_second() {
    const long probe = 20000000;
    auto start = std::chrono::steady_clock::now();
    calibration_loop(probe);
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                  std::chrono::steady_clock::now() - start).count();
    if (us < 1000) us = 1000;
    return static_cast<long>(probe * 1000000.0 / static_cast<double>(us));
}

// Counts samples by leaf frame. Output is root-first, so the leaf is last.
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

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::cerr << "test_sampler_accuracy FAIL: " << msg << "\n";       \
            return 1;                                                          \
        }                                                                      \
    } while (0)

int main() {
    const std::string outputPath = "tests/sampler_accuracy_output.txt";
    std::remove(outputPath.c_str());

    calibration_loop(20000000);
    const long unit = iterations_per_second();   // one second of work

    const int rounds = 3;
    {
        Sampler sampler(outputPath, 200);
        for (int r = 0; r < rounds; ++r) {
            func_a(3 * unit / rounds);
            func_b(2 * unit / rounds);
            func_c(1 * unit / rounds);
        }
    }

    auto counts = count_self_samples(outputPath);
    std::remove(outputPath.c_str());

    const int total = counts["func_a"] + counts["func_b"] + counts["func_c"];
    CHECK(total > 0, "no samples landed in any spin function -- is the leaf PC "
                     "being captured from ucontext?");
    CHECK(total >= 200, "too few samples (" << total << ") for a meaningful ratio");

    const double pct_a = 100.0 * counts["func_a"] / total;
    const double pct_b = 100.0 * counts["func_b"] / total;
    const double pct_c = 100.0 * counts["func_c"] / total;

    std::cout << "func_a: " << counts["func_a"] << " samples (" << pct_a << "%)\n"
              << "func_b: " << counts["func_b"] << " samples (" << pct_b << "%)\n"
              << "func_c: " << counts["func_c"] << " samples (" << pct_c << "%)\n"
              << "total:  " << total << " samples\n";

    const double tolerance = 5.0;
    CHECK(std::fabs(pct_a - 50.0) < tolerance, "func_a share off: " << pct_a);
    CHECK(std::fabs(pct_b - 33.3) < tolerance, "func_b share off: " << pct_b);
    CHECK(std::fabs(pct_c - 16.7) < tolerance, "func_c share off: " << pct_c);

    std::cout << "test_sampler_accuracy: PASS\n";
    return 0;
}
