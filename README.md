# COMP2002 Operating Systems — Parallel Mergesort

## Project Number/Title

* Authors: Zishi Wang (a1924840), Qimu Liu (a1898827), Wujiacheng Ding (a1939784)
* Group name: Group 15

### Division of work

* **Qimu Liu (a1898827)** — serial sorting: `merge()` and the recursive
  `my_mergesort()` used as the base case when the thread-creation depth is
  reached.
* **Wujiacheng Ding (a1939784)** — threading: `parallel_mergesort()`,
  `buildArgs()`, `join_or_exit()`, and the fallback path that sorts a half
  serially when `pthread_create()` fails.
* **Zishi Wang (a1924840)** — build and integration, the correctness harness
  that compares the result against `qsort`, performance benchmarking on 24 and
  4 cores, memory-safety (ASan/UBSan) and thread-safety (TSan) testing, and this
  report.

## Overview

This project implements a multithreaded merge sort that sorts an array of `int`
in shared memory. The array is split in half repeatedly; each half is handed to
a newly created POSIX thread, and the recursion stops once a user-supplied
`cutoff` level is reached, at which point the remaining subarray is sorted by a
plain recursive `my_mergesort`. A thread only merges its two halves after both
children have been joined, so no two threads ever write to the same interval.

## Manifest

| File | Purpose |
| --- | --- |
| `Makefile` | Build rules; `-O2`, strict warnings, dependency tracking |
| `mergesort.c` | `merge`, `my_mergesort`, `parallel_mergesort`, `buildArgs` |
| `mergesort.h` | Prototypes, `struct argument`, and the `A`, `B`, `cutoff` globals |
| `test-mergesort.c` | Supplied driver: allocates, fills, sorts, times, checks sortedness |
| `README.md` | This report |

`test-mergesort.c` and `mergesort.h` is delivered unmodified, as required by the specification.

## Building the project

```sh
make
```

This produces `./test-mergesort` in the project directory. The only requirement
is a C compiler with POSIX threads and `<error.h>`; the default `CC` is `gcc`.
`make clean` removes the object files, the dependency files and the executable.

The build uses `-O2 -Wall -Wextra -Wpointer-arith -Wstrict-prototypes
-std=gnu89 -fPIC -MMD -MP -pthread` and compiles with **no warnings**.

## Features and usage

```sh
./test-mergesort <n> <cutoff> <seed>
```

Example: `./test-mergesort 100000000 5 1`

* `n` — number of random integers to sort.
* `cutoff` — the depth at which threads stop being created. `0` performs no
  threading at all and gives the serial baseline; each additional level doubles
  the number of threads, so `cutoff = k` creates `2^(k+1) - 2` threads.
* `seed` — seed for `random()`, so runs are reproducible.

The program prints the elapsed wall-clock time for the sort. It does not check
correctness itself, which is why correctness is established separately, against
`qsort`, in the section below.

Robustness: if `pthread_create` fails (for example under a process limit), the
half that could not be given a thread is sorted serially in the calling thread
instead, so the final result is still correct. If `malloc` fails, the program
reports the error and exits rather than dereferencing a null pointer.

## Testing

All timings below were taken on a WSL2 Ubuntu guest (Linux 6.6.87.2-1) with 24
logical cores and `gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`, `n = 10^8`,
`seed = 1`.

### 1. Compilation

`make clean && make` completes with zero warnings under `-Wall -Wextra
-Wpointer-arith -Wstrict-prototypes`.

### 2. Correctness against a reference implementation

A separate driver (kept outside the submission) generates the same random input
twice:

- array `A` is sorted by `parallel_mergesort`;
- array `ref` is sorted by glibc `qsort`.

The two are then compared with `memcmp` over the whole array. Because the
comparison is byte-for-byte and the input is regenerated with the same seed, it
checks two properties at once: the result is ascending, **and** it is a
permutation of the original input. Checking only "is sorted" would not catch a
merge that silently duplicates or drops an element.

The full matrix was
`n ∈ {2, 3, 17, 1000, 100000, 1000000, 10000000}` × `cutoff ∈ {0…6}` ×
`seed ∈ {1, 7, 42}` = **147 cases, all passing**.

The three small sizes are deliberate. `n = 2` and `n = 3` exercise the
`left >= right` base case and an odd-length split; `n = 17` produces a split
that is not a power of two, so the two halves differ in size — the case where an
off-by-one in `mid` or in the merge bounds would show up.

### 3. The supplied driver

`test-mergesort.c` is delivered unmodified and was also run directly, because it
is the interface the marker uses. At `n = 1,000,000`, `cutoff = 3`, `seed = 1`:

```
$ ./test-mergesort 1000000 3 1
Sorting 1000000 elements took 0.01 seconds.
```

Every run used for the timings in section 7 printed a timing line; none printed
`sorting failed!!!!`, which is the only failure message the driver emits.

Two things are worth separating here. First, the driver is the **only** harness
that exercises `buildArgs()`: it constructs the root argument with
`buildArgs(0, n - 1, 0)`, so the allocation and its failure branch are covered
there and nowhere else. Our `verify.c` builds the root argument on the stack and
never calls `buildArgs()`. Second, the driver only checks that the output is
non-decreasing, so it cannot be the basis of the correctness claim — the
permutation check in section 2 is. The two harnesses cover different holes.

### 4. Address and undefined-behaviour sanitizers

`gcc -fsanitize=address,undefined -g -O1` at `n = 2,000,000` and `cutoff = 3`
runs to completion with no error report.

An initial run did report a leak of 8 MB. Instrumenting the driver showed the
allocation was the driver's own reference array, not the library: every
`malloc` performed by `buildArgs` is released in `parallel_mergesort` once both
children have been joined. After freeing the driver's buffers, the sanitizer run
is clean. This distinction matters — a leak report pointing at a test harness is
not evidence of a bug in the code under test.

The statement above applies to the separate driver. Because that driver builds
the root argument on the stack, it never enters `buildArgs()`. The supplied
`test-mergesort.c` was therefore rebuilt with the same flags and run at
`n = 2,000,000`, `cutoff = 3`:

```
$ ./asan-driver 2000000 3 1
Sorting 2000000 elements took 0.07 seconds.
```

It reported nothing. This run is the one that instruments the `malloc` inside
`buildArgs()` and the `exit()` on its failure branch, i.e. the code path no
other harness reaches.

### 5. Thread sanitizer

`gcc -fsanitize=thread -g -O1`, run at `cutoff = 4` (30 threads) and
`cutoff = 6` (126 threads) with `n = 200,000`, reports no data race.

On this particular kernel the tool initially aborted with
`FATAL: ThreadSanitizer: unexpected memory mapping`, which is a known conflict
between TSan's shadow memory layout and a high `vm.mmap_rnd_bits`. Running the
binary through `setarch $(uname -m) -R` (disabling address-space randomization
for that process only) resolves it without changing the program.

### 6. Failure injection

Two attempts were made to force `pthread_create` to fail. On this WSL kernel
neither `RLIMIT_NPROC` (which blocks `fork` but apparently not `clone` with
`CLONE_THREAD`) nor `RLIMIT_AS` produced a clean `EAGAIN`; the latter instead
terminated the process. The fallback path is therefore verified by construction
rather than by experiment: on a non-zero status the parent calls
`my_mergesort` on the affected half, and that half is never joined or freed by
the parent, so no partially written data can be read.

### 7. Performance

Median of three runs, `n = 10^8`, `seed = 1`, seconds.

| cutoff | threads | 24 cores | speed-up | 4 cores (`taskset -c 0-3`) | speed-up |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 0 | 1 | 7.63 | 1.00× | 7.71 | 1.00× |
| 1 | 2 | 4.14 | 1.84× | 4.25 | 1.81× |
| 2 | 6 | 2.30 | 3.32× | 2.47 | 3.12× |
| 3 | 14 | 1.42 | 5.37× | 2.62 | 2.94× |
| 4 | 30 | 0.92 | 8.29× | 2.51 | 3.07× |
| 5 | 62 | **0.78** | **9.78×** | 2.49 | 3.10× |
| 6 | 126 | 0.82 | 9.30× | 2.49 | 3.10× |

Three observations:

**The ceiling is 13.3×, not 24×.** The parallel part of the algorithm stops at
depth `cutoff`; the merges above it are performed by a single parent thread
after joining. For `n = 10^8` the serial work is therefore bounded below by
`log2(n)` merge passes of `n` elements each, giving an Amdahl limit of about
`log2(10^8)/2 ≈ 13.3×` on this input. The measured 9.78× is 74 % of that limit.

**Efficiency is consistent across core counts.** On 24 cores the result reaches
74 % of its algorithmic ceiling; on 4 cores it reaches 3.12× against a 4×
ceiling, which is 78 %. Two very different machines converge on roughly three
quarters of their respective limits, which points at a common cause — the serial
merge path and memory bandwidth, `n = 10^8` being an 800 MB working set that
does not fit in cache — rather than at scheduling or lock contention. There are
no locks in the implementation at all; the only synchronisation is
`pthread_join`.

**The best cutoff is a function of the core count.** With 24 cores,
`cutoff = 5` is fastest (0.78 s) and also the most stable: 0.81 / 0.78 / 0.77,
a spread of ±2.5 %. `cutoff = 6` produces 126 threads on 24 cores — a 5.25×
oversubscription — and its three runs were 0.81 / 3.82 / 0.82: the median looks
fine, but one run took five times longer. With only 4 cores, `cutoff = 2`
(6 threads) already reaches 3.12× and every higher cutoff sits on a plateau
between 2.47 s and 2.62 s. Threads created beyond that point buy nothing.

Two outliers were deliberately kept rather than removed: `cutoff = 0`, run 2
took 10.14 s (a serial 800 MB pass competing with background load), and
`cutoff = 6`, run 2 took 3.82 s. Reporting medians without showing the spread
would hide exactly the argument for choosing `cutoff = 5`.

Caveat: `taskset -c 0-3` only restricts affinity on a 24-core machine; the L3
cache and memory bandwidth remain those of the larger system. The 4-core numbers
are therefore an optimistic bound for a real 4-vCPU host.

## Known Bugs

* **`cutoff = 1` cannot reach the 2× target.** Two threads give a theoretical
  maximum of 2×, and the top-level merge of `n` elements runs serially
  afterwards, so the measured value is 1.84× (24 cores) and 1.81× (4 cores).
  This is inherent to the algorithm rather than a defect, but it is a real
  constraint on the cutoff range.
* **Memory use is 8n bytes.** Two full arrays are held, so `n = 10^8` needs
  800 MB. The sort is not in place.
* **Under an artificially low `RLIMIT_AS` the process crashed with SIGSEGV**
  instead of failing cleanly. Both allocation sites in the library are checked,
  so the fault is most likely in the C library or loader under a pathological
  address-space limit; it was observed once and is not reproducible under
  normal conditions, and it was not investigated further.
* **The failure branches are untested.** When the `malloc` inside `buildArgs`
  returns `NULL` the library prints an error and exits; when `pthread_create`
  fails, that half is sorted serially instead. Neither branch has ever been
  executed. Exhausting memory could not be induced reliably: `ulimit -v` was
  ignored by the kernel under test, `prlimit --as` crashed in the loader before
  the code under test ran, and `RLIMIT_NPROC` does not count threads on that
  kernel, so it could not force `pthread_create` to fail either. These paths are
  therefore verified by reading, not by execution.
* **Thread count is unbounded by the core count.** `cutoff` alone decides how
  many threads are created, so a large `cutoff` on a small machine
  oversubscribes. An improvement would be to clamp the effective cutoff at
  `log2(nproc) + 1`.

## Reflection and Self Assessment

The largest obstacle had nothing to do with the algorithm. The first working
version was run on macOS, which has no `<error.h>`; the header had to be
commented out, so that build was not the file that would be submitted. Moving
to WSL took a full evening because the Windows optional component
`VirtualMachinePlatform` was disabled: WSL2 could list its distributions but
refused to start them, and Docker Desktop failed for the same reason. Enabling
the component, rebooting and rebuilding on a real Linux kernel was what finally
gave a trustworthy build.

The second lesson was about tooling that reports failures that are not yours.
ThreadSanitizer aborted before running a single line of the program, because of
an address-space-randomisation conflict; the fix was `setarch -R`. AddressSanitizer
then reported an 8 MB leak, which turned out to live entirely in the test driver.
Both cases were resolved by reading the report carefully instead of assuming the
code under test was at fault.

The part that finally "clicked" was why the speed-up saturates well below the
core count. Once the cutoff was varied systematically and the Amdahl bound for
this input was computed, the 9.78× figure stopped looking like a disappointing
result and started looking like 74 % of the achievable maximum. That reframing —
measure, compute the bound, compare against the bound rather than against the
number of cores — is the most transferable thing learned in this project.

If the work were repeated, two things would be done earlier: establish a Linux
build on day one, and write the comparison against `qsort` before tuning
anything, because a performance number is worthless until correctness is
established.

## Sources Used

* `pthread_create(3)`, `pthread_join(3)`, `qsort(3)` — Linux man pages.
* ThreadSanitizer and AddressSanitizer documentation.
* Gemini model gemini-3-web for markdown file format help.
* ChatGPT model GPT-6-Astra for expansion code speed test help and expanded README template idea after review.
* `setarch(1)` / `taskset(1)` — Linux man pages.
* Course lecture notes on merge sort and Amdahl's law.
