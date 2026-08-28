# Pinned so results are reproducible between machines and between people.
# -fno-omit-frame-pointer keeps the caller chain walkable; the leaf itself comes
# from the interrupted PC, which needs no flag.
CXX      ?= clang++
CXXFLAGS ?= -std=c++20 -O1 -g -Wall -Wextra -fno-omit-frame-pointer

AGGREGATOR_SRC := main.cpp aggregator.cpp
SAMPLER_SRC    := sampler.cpp

TESTS := tests/test_sampler_accuracy tests/test_leaf_visibility

.PHONY: all tests test clean

all: strobe

strobe: $(AGGREGATOR_SRC) aggregator.h
	$(CXX) $(CXXFLAGS) $(AGGREGATOR_SRC) -o $@

tests: $(TESTS)

tests/%: tests/%.cpp $(SAMPLER_SRC) sampler.h
	$(CXX) $(CXXFLAGS) $< $(SAMPLER_SRC) -o $@

test: tests
	@for t in $(TESTS); do echo "== $$t =="; ./$$t || exit 1; done
	@echo "all tests passed"

clean:
	rm -rf strobe $(TESTS) *.dSYM tests/*.dSYM tests/*_output.txt
