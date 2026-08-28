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
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr int kMaxDepth = 64;
constexpr int kMaxSamples = 20000;
constexpr int kFramesToSkip = 2;   // handle_sigprof, _sigtramp

struct Sample {
    int depth;
    void* frames[kMaxDepth];       // frames[0] is the leaf
};

// All handler-visible storage is allocated before the timer is ever armed.
Sample g_samples[kMaxSamples];

std::atomic<size_t> g_next{0};
std::atomic<size_t> g_dropped{0};
std::atomic<bool> g_active{false};

// Must be async-signal-safe (no stdio, memory allocation, etc)
void handle_sigprof(int, siginfo_t*, void* ucontext) {
    size_t i = g_next.fetch_add(1, std::memory_order_relaxed);

    if (i >= kMaxSamples) {
        g_dropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    void* raw[kMaxDepth + kFramesToSkip];
    int n = backtrace(raw, kMaxDepth + kFramesToSkip);
    if (n <= kFramesToSkip) {
        g_dropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    Sample& s = g_samples[i];

    const ucontext_t* uc = static_cast<const ucontext_t*>(ucontext);
    s.frames[0] = reinterpret_cast<void*>(arm_thread_state64_get_pc(uc->uc_mcontext->__ss));

    int chain = std::min(n - kFramesToSkip, kMaxDepth - 1);
    std::memcpy(s.frames + 1, raw + kFramesToSkip, static_cast<size_t>(chain) * sizeof(void*));
    s.depth = 1 + chain;
}

std::string resolve(void* addr, const void*& symStart) {
    Dl_info info{};
    symStart = nullptr;
    if (!dladdr(addr, &info) || !info.dli_sname) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%p", addr);
        return buf;
    }
    symStart = info.dli_saddr;

    int status = 0;
    char* demangled = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
    std::string name = (status == 0 && demangled) ? demangled : info.dli_sname;
    std::free(demangled);
    return name;
}

} // namespace

Sampler::Sampler(std::string outputPath, int hz) : outputPath_(std::move(outputPath)) {
    if (g_active.exchange(true)) {
        std::cerr << "Sampler: another Sampler is already active; they share one buffer\n";
    }

    g_next.store(0);
    g_dropped.store(0);

    void* warmup[4];
    backtrace(warmup, 4);

    struct sigaction sa{};
    sa.sa_sigaction = handle_sigprof;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;   // SA_RESTART: don't break the target's syscalls
    sigemptyset(&sa.sa_mask);
    sigaction(SIGPROF, &sa, nullptr);

    struct itimerval timer{};
    timer.it_value.tv_usec = 1000000 / hz;
    timer.it_interval.tv_usec = 1000000 / hz;
    setitimer(ITIMER_PROF, &timer, nullptr);
}

Sampler::~Sampler() {
    struct itimerval off{};
    setitimer(ITIMER_PROF, &off, nullptr);

    struct sigaction ign{};
    ign.sa_handler = SIG_IGN;
    sigemptyset(&ign.sa_mask);
    sigaction(SIGPROF, &ign, nullptr);

    sigset_t block;
    sigemptyset(&block);
    sigaddset(&block, SIGPROF);
    sigprocmask(SIG_BLOCK, &block, nullptr);

    size_t n = std::min(g_next.load(), static_cast<size_t>(kMaxSamples));

    std::ofstream out(outputPath_);
    std::vector<std::string> names;
    for (size_t i = 0; i < n; ++i) {
        const Sample& s = g_samples[i];

        names.clear();
        const void* leafSym = nullptr;
        for (int f = 0; f < s.depth; ++f) {
            const void* sym = nullptr;
            std::string name = resolve(s.frames[f], sym);
            if (f == 1 && sym != nullptr && sym == leafSym) continue;
            if (f == 0) leafSym = sym;

            names.push_back(std::move(name));
        }
        if (names.empty()) continue;

        for (size_t k = names.size(); k-- > 0; ) out << names[k] << (k ? ";" : "\n");
    }

    if (g_dropped.load() > 0) {
        std::cerr << "Sampler: dropped " << g_dropped.load() << " samples (buffer full)\n";
    }

    sigprocmask(SIG_UNBLOCK, &block, nullptr);
    g_active.store(false);
}
