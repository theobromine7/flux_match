# Phase 0 — Build and understand

Walkthrough of what was done, why, and how to verify it. Use this as a review
document: every change below should have a reason you can restate in your own
words.

Status on 2026-10-26:

| Step | Status |
| --- | --- |
| 0.1 Build/test/run docs | Done (`docs/BUILD.md`) |
| 0.2 Separate targets | Done (core library + 3 executables; bench comes in Phase 1) |
| 0.3 Debug/release presets | Done (`CMakePresets.json`) |
| 0.4 Linux/WSL verification | CI job added; green run still required |
| 0.5 Environment record | Done (`docs/ENVIRONMENT.md`) |

The exit gate is: *a fresh checkout builds and tests on Windows and Linux/WSL
using only the docs.* Windows is proven. Linux is proven as soon as the first
CI run is green.

## What changed

| File | Change | Why |
| --- | --- | --- |
| `vcpkg.json` | Added manifest with `builtin-baseline` | Dependencies declared in-repo and pinned, instead of relying on an ambient vcpkg tree |
| `CMakePresets.json` | Added Windows debug/release configure, build, test presets | Makes the first configure correct by construction; no remembered flags |
| `CMakeLists.txt` | 4 real targets; `PUBLIC include`; optional market-data | One library, thin executables, no duplicate compilation |
| `CMakeLists.txt` | `cmake_minimum_required(VERSION 3.25)` | Preset schema v6 requires 3.25 |
| `src/main.cpp` | Portable `readTSC` shim | Removes the hard dependency on MSVC's `<intrin.h>` |
| `.github/workflows/cpp.yml` | Linux build + test job | Executes the Linux instructions on a clean machine |
| `docs/BUILD.md` | Full prerequisites, commands, troubleshooting, limitations | A fresh user can build without asking questions |
| `docs/ENVIRONMENT.md` | Versions, machine, caveats | Benchmarks and experiments are relative to this environment |
| `.gitattributes` | `* text=auto` | Stops CRLF/LF churn between Windows and Linux |

## 1. Reproducible dependencies

`CMakeLists.txt` used to call `find_package(Boost CONFIG REQUIRED)`, but nothing
in the repository said where Boost came from. The old `build/CMakeCache.txt`
proved it: an absolute path into `C:/Users/kazia/Projects/vcpkg`, passed via
`-DCMAKE_TOOLCHAIN_FILE`. A fresh clone could not reproduce that.

Now:

- `vcpkg.json` lists `boost-beast`, `boost-json`, `openssl`.
- `builtin-baseline` pins the vcpkg port versions to one commit.
- `CMakePresets.json` passes the toolchain from `$env{VCPKG_ROOT}`.
- The manifest installs dependencies into `build/<preset>/vcpkg_installed/`, so
  each build directory is self-contained.

Key idea: **the toolchain path is configuration, not code.** It belongs in the
preset/environment, not hardcoded in `CMakeLists.txt`. That is why the earlier
`if(NOT CMAKE_TOOLCHAIN_FILE) FATAL_ERROR` guard was removed — it would also
block a valid Linux build that uses system packages.

## 2. Targets

| Target | Kind | Contents |
| --- | --- | --- |
| `fluxmatch_core` | static library | `Order`, `OrderBook`, `Trade`, `MatchingEngine` |
| `fluxmatch_demo` | executable | `src/main.cpp` (currently the benchmark) |
| `fluxmatch_marketdata` | executable | Binance WebSocket/HTTP client |
| `fluxmatch_tests` | executable | `tests/OrderBookTests.cpp` |

`fluxmatch_core` exposes `include/` as a `PUBLIC` include directory, so all
includes are written as `<fluxmatch/OrderBook.hpp>`. This is what makes the
library usable from another project without header-name collisions.

Boost and OpenSSL are only searched inside `FLUXMATCH_BUILD_MARKET_DATA`, which
defaults to `ON`. Turning it off gives a core-only build with no external
dependencies:

```powershell
cmake -S . -B build/core-only -DFLUXMATCH_BUILD_MARKET_DATA=OFF
```

## 3. Presets and multi-config generators

The Visual Studio generator is multi-configuration: one build directory, and
`--config Debug|Release` selects the output. `CMAKE_BUILD_TYPE` is ignored.
That is why `buildPresets` and `testPresets` specify `"configuration"`.

Preset schema `"version": 6` requires CMake **3.25**. The old
`cmake_minimum_required(VERSION 3.20)` claimed support for a version that cannot
read the presets, so it was bumped.

## 4. Portability: the `intrin.h` problem

`src/main.cpp` contained:

```cpp
#include <intrin.h>
return __rdtscp(&aux);
```

`<intrin.h>` exists only on MSVC. The Linux build therefore failed at compile
time — the Linux section of the docs was a claim, not a fact.

The fix is a preprocessor guard, because the preprocessor removes text before
the compiler sees it:

```cpp
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <intrin.h>
#define FLUXMATCH_HAS_RDTSCP 1
#elif (defined(__GNUC__) || defined(__clang__)) && \
    (defined(__x86_64__) || defined(__i386__))
#include <x86intrin.h>
#define FLUXMATCH_HAS_RDTSCP 1
#endif
```

`__rdtscp` exists on both compilers for x86; only the header differs. On
non-x86 there is a `std::chrono::steady_clock` fallback.

This is deliberately temporary. Phase 1.1 moves the benchmark into
`benchmarks/bench_main.cpp`; Phase 1.8 replaces this shim with a documented
timer abstraction and measures its overhead.

## 5. Why CI is the Linux verification

A local WSL or Docker build can pass because packages were installed manually
and environment variables were exported. A CI runner starts clean: no Boost, no
OpenSSL, no cached build, no `VCPKG_ROOT`. A green run proves that the
repository plus its instructions are sufficient — exactly the Phase 0 exit gate.

The workflow installs `libboost-all-dev` and `libssl-dev`, configures with
CMake in Release, builds in parallel, and runs `ctest`. It also satisfies the
first part of Phase 9.1 (C++ CI).

Disk note: this machine had ~3.7 GiB free when Phase 0 was recorded, and only
the `docker-desktop` WSL distro is installed. Running a local Linux container
would consume roughly 1–2 GiB, so Linux is verified in CI instead. Windows CI is
deferred to Phase 9, where vcpkg binary caching is worth setting up.

## Verification

Windows, after the portability change:

```text
cmake --preset windows-release        -> Configure done
cmake --build --preset windows-release -> fluxmatch_core.lib, demo, marketdata, tests built
ctest --preset windows-release        -> 1/1 passed
```

Linux: `workflow_dispatch`/push run of `cpp` workflow — pending. This is the
only remaining Phase 0 checkbox.

## Review questions (you should be able to answer these)

1. Why is `builtin-baseline` needed if `vcpkg.json` already lists packages?
2. Why does `fluxmatch_core` use `PUBLIC include` instead of `include/fluxmatch`?
3. Why is `CMAKE_BUILD_TYPE` irrelevant with the Visual Studio generator?
4. Why does the `#ifdef` fix portability rather than just hiding an error?
5. What does a clean CI runner prove that a working local build does not?
6. Why is the `FATAL_ERROR` toolchain guard a problem for Linux?
7. Why is `Boost::boost` the wrong link target for code that uses JSON?

## Next

Phase 1.1: move the benchmark out of `main.cpp` into
`benchmarks/bench_main.cpp`, add the `fluxmatch_bench` target, and leave
`main.cpp` as a small demo. The portable timer shim goes with the benchmark and
becomes a proper timer module in Phase 1.8.
