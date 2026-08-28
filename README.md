# strobe

A sampling CPU profiler for macOS on Apple silicon. Link it into a program, wrap
the region you care about, and get a flame-graph-ready breakdown of where the
time went.

```cpp
#include "sampler.h"

int main() {
    Sampler sampler("profile.folded", 200);   // 200 Hz
    run_the_workload();
}                                             // writes profile.folded on scope exit
```

```
$ strobe profile.folded
decode_body(long): self=170 total=170
parse_header(long): self=69 total=69
checksum(long): self=30 total=30
start: self=0 total=269
main: self=0 total=269
process(long): self=0 total=269
```

`self` is samples where the function was executing its own instructions.
`total` is samples where it appeared anywhere on the stack. A function with high
`total` and near-zero `self` is a caller, not a bottleneck.

## Why not just call `backtrace()`?

Because it silently loses the hottest function in your program.

`backtrace()` walks the frame-pointer chain. A function that calls nothing never
sets up a frame pointer — the compiler just bumps the stack pointer:

```
__Z4spinl:
    sub  sp, sp, #32       ; allocates stack...
    str  x0, [sp, #24]     ; ...but no `stp x29, x30`, so no chain entry
```

That happens at **every** optimization level, `-O0` included. So the leaf is
absent from the chain and its time gets billed to its caller. On a test workload
that spends essentially all of its time in one such function:

```
backtrace() only:   leaf_only() is the leaf in    0 / 171 samples
+ interrupted PC:   leaf_only() is the leaf in  233 / 233 samples
```

strobe takes the innermost frame from the **interrupted program counter**, read
out of `ucontext_t` in the signal handler, and uses the frame chain only for the
callers above it:

```cpp
s.frames[0] = (void*)arm_thread_state64_get_pc(uc->uc_mcontext->__ss);
s.depth     = 1 + backtrace_skipping_handler_frames(s.frames + 1);
```

`tests/test_leaf_visibility.cpp` is a regression test for exactly this.

## How it works

| Stage | What happens |
| --- | --- |
| Arm | `sigaction` installs a `SIGPROF` handler; `setitimer(ITIMER_PROF)` fires it on a CPU-time interval |
| Capture | The handler stores raw addresses into a pre-allocated buffer — no allocation, no locks, no stdio |
| Symbolize | On destruction, `dladdr` + `abi::__cxa_demangle` turn addresses into names |
| Emit | One collapsed stack per line, root-first, semicolon-separated |

The handler is async-signal-safe by construction. It interrupts the program at
an arbitrary instruction — possibly mid-`malloc` — so it touches only
pre-allocated memory and a lock-free `std::atomic<size_t>`. Symbolization is
deferred to shutdown precisely because it allocates.

## Output format

Collapsed ("folded") stacks, root first:

```
start;main;process(long);decode_body(long)
start;main;process(long);decode_body(long)
start;main;process(long);checksum(long)
```

This is the format [`flamegraph.pl`](https://github.com/brendangregg/FlameGraph)
consumes, so `sort profile.folded | uniq -c | flamegraph.pl` works directly.

## Using it

### CMake (FetchContent)

```cmake
if (APPLE)
    include(FetchContent)
    FetchContent_Declare(strobe
        GIT_REPOSITORY https://github.com/SilasTeague/sampling-profiler.git
        GIT_TAG <pin-a-sha>
    )
    set(STROBE_BUILD_CLI OFF CACHE BOOL "" FORCE)
    set(STROBE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(strobe)

    target_link_libraries(my_target PRIVATE strobe::sampler strobe::aggregator)
endif()
```

`strobe::sampler` sets `-fno-omit-frame-pointer` as a `PUBLIC` compile option:
the unwinder walks the *caller's* frames, so every consumer needs it, not just
`sampler.cpp`.

### Directly

```
make            # builds ./strobe, the report CLI
make test       # builds and runs both tests
```

## Limitations

- **macOS on arm64 only.** `arm_thread_state64_get_pc` and the Mach thread state
  layout are Apple-specific. The CMake target only exists under `if (APPLE)`.
- **In-process.** It profiles the program it is linked into. It cannot attach to
  a running PID.
- **Inlined frames are invisible.** Frame-pointer unwinding sees machine stacks,
  not source stacks. Recovering inlined frames needs DWARF inline records.
- **One `Sampler` at a time.** A signal handler takes no user argument, so its
  storage is file-scope. Constructing a second live `Sampler` warns on stderr.
- **Sample rate is approximate.** Treat results as proportions, never as
  milliseconds.
- **Buffer is bounded** at 20,000 samples; overflow is counted and reported
  rather than silently dropped.

## License

MIT — see [LICENSE](LICENSE).
