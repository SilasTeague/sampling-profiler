#include "aggregator.h"
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: ./strobe [filename]" << "\n";
        return 1;
    }

    Aggregator aggregator(argv[1]);

    aggregator.processFile();
    aggregator.output();

    return 0;
}
