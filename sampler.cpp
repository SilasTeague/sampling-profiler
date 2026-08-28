#define _XOPEN_SOURCE 700          // must precede includes: unlocks ucontext on Darwin
#define _DARWIN_C_SOURCE
#include "sampler.h"
#include <csignal>
#include <sys/time.h>
#include <execinfo.h>
#include <dlfcn.h>
#include <cxxabi.h>
#include <ucontext.h>
#include <mach/mach.h>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <cstring>
#include <fstream>
#include <vector>
#include <algorithm>

namespace {

constexpr int kMaxDepth = 64;
constexpr int kMaxSamples = 20000;

struct Sample {
    int depth;
    void* frames[kMaxDepth];
};

Sample g_samples[kMaxSamples];

std::atomic<size_t> g_next{0};
std::atomic<size_t> g_dropped{0};

// Must be async-signal-safe (no stdio, memory allocation, etc)
void handle_sigprof(int, siginfo_t*, void* ucontext) {
    size_t i = g_next.fetch_add(1, std::memory_order_relaxed);

    if (i >= kMaxSamples) { 
        g_dropped.fetch_add(1, std::memory_order_relaxed); 
        return; 
    }
    Sample& s = g_samples[i];
    ucontext_t* uc = static_cast<ucontext_t*>(ucontext);
    s.frames[0] = reinterpret_cast<void*>(arm_thread_state64_get_pc(uc->uc_mcontext->__ss));
    s.depth = 1 + backtrace(s.frames + 1, kMaxDepth - 1);
}

} // namespace

Sampler::Sampler(std::string outputPath, int hz) : outputPath_(std::move(outputPath)) {
    struct itimerval timer{};

    struct sigaction sa{};
    sa.sa_sigaction = handle_sigprof;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGPROF, &sa, nullptr);

    timer.it_value.tv_usec = 1000000 / hz;
    timer.it_interval.tv_usec = 1000000 / hz;

    setitimer(ITIMER_PROF, &timer, NULL);
}

Sampler::~Sampler() {
    struct itimerval off{};
    setitimer(ITIMER_PROF, &off, nullptr);

    size_t n = std::min(g_next.load(), static_cast<size_t>(kMaxSamples));

    std::ofstream out(outputPath_);
    for (size_t i = 0; i < n; ++i) {
        const Sample& s = g_samples[i];
        for (int f = 0; f < s.depth; ++f) {
            if (f) out << ';';
            Dl_info info{};
            if (dladdr(s.frames[f], &info) && info.dli_sname) {
                int status = 0;
                char* demangled = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
                out << (status == 0 ? demangled : info.dli_sname);
                free(demangled);
            } else {
                out << s.frames[f];
            }
        }
        out << '\n';
    }

    if (g_dropped.load() > 0) {
        std::cerr << "Sampler: dropped " << g_dropped.load() << " samples (buffer full)\n";
    }
}

