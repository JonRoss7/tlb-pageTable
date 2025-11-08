# tlb-pageTable

tlb-pageTable is a simple simulator for virtual-to-physical address translation with optional TLB and multi-level page table support. It is intended for lab/testing of page table behavior, TLB hits/misses, and frame eviction.

## Features
- 2-level page table implementation (L1 / L2) in [`main.cpp`](main.cpp)
- Optional TLB with LRU insertion logic implemented in the [`TLB`](main.cpp) class
- Page fault handling and random victim selection for a fixed physical-frame pool (256 frames when enabled)
- Small address generator utility: [`generate_addresses.cpp`](generate_addresses.cpp) that writes to [`addresses.txt`](addresses.txt)

## Repository layout
- main.cpp — simulator and translation routines (see [`translate`](main.cpp) and [`translate_part1`](main.cpp))
- generate_addresses.cpp — creates `addresses.txt` (random + "hot" region)
- addresses.txt — sample input of virtual addresses
- Makefile — build / run / clean targets
- lab — compiled simulator binary (built by Makefile)
- generate — compiled address generator binary (built by Makefile)

## Build
Requires a standard g++ toolchain.

From repository root:
```sh
make all
```