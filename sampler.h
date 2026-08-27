#pragma once

#include <string>

// Creates a SIGPRPOF timer on construction and outputs a file on deconstruction
class Sampler {
public:
    explicit Sampler(std::string outputPath, int hz=100);
    ~Sampler();

    // Do not allow Samplers to be copied
    Sampler(const Sampler&) = delete;
    Sampler& operator=(const Sampler&) = delete;
private:
    std::string outputPath_;
};