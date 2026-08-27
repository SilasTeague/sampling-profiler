#include "sampler.h"
#include <atomic>
#include <sys/time.h>
#include <iostream>
#include <csignal>

std::atomic<size_t> count{0000};

void handle_sigprof(int sig) {
    count++;
}

Sampler::Sampler(std::string outputPath, int hz) : outputPath_(std::move(outputPath)) {
    struct itimerval timer{};

    signal(SIGPROF, handle_sigprof);

    timer.it_value.tv_usec = 1000000 / hz;
    timer.it_interval.tv_usec = 1000000 / hz;

    setitimer(ITIMER_PROF, &timer, NULL);
}

Sampler::~Sampler() {
    std::cout << count << "\n";
}

