# Environment

Recorded on 2026-10-26. Update this file when the toolchain changes; every
benchmark and experiment in this repository is only meaningful relative to the
environment described here.

## Development machine

| Item | Value |
| --- | --- |
| OS | Windows 11 Home Single Language, 10.0.26200 (64-bit) |
| CPU | 11th Gen Intel Core i3-1115G4 @ 3.00 GHz, 2 cores / 4 threads |
| Free disk at recording | ~3.7 GiB — low; avoid large local Linux images |
| Shell | PowerShell 7 / Windows PowerShell |

The CPU has only two physical cores. This matters for Phase 4: thread counts
above 2 oversubscribe the machine, and benchmarks must not be compared with
results from a larger machine. It also makes pinning/affinity experiments noisy.

## Compiler and build tools

| Item | Value |
| --- | --- |
| Compiler | MSVC 19.44.35228.0 |
| Visual Studio | 2022 Build Tools 17.14.37516.0 |
| CMake | 3.31.6-msvc6 (bundled with Visual Studio) |
| Generator | Visual Studio 17 2022, architecture x64 |
| C++ standard | C++20 (`CMAKE_CXX_EXTENSIONS OFF`) |

CMake is not a standalone install. It ships under
`C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin`.
A Visual Studio update can change this version; record it again if that happens.
`cmake_minimum_required` is 3.25 because `CMakePresets.json` uses schema v6.

## Dependencies

Managed by vcpkg in manifest mode (`vcpkg.json`), installed inside each build
directory under `vcpkg_installed/`.

| Item | Value |
| --- | --- |
| vcpkg root | `C:\Users\kazia\Projects\vcpkg` (`VCPKG_ROOT`, user environment) |
| vcpkg version | 2026-07-27, commit `98d7cb0cf1f4686a3e43aa5672b6230c1d56bce8` |
| vcpkg baseline | `127402f1c75bb3d5ff6bce04b285faa4930a5aca` |
| Triplet | `x64-windows` |
| Boost | 1.92.0 (`boost-beast`, `boost-json`) |
| OpenSSL | 3.6.3 |

`builtin-baseline` pins the manifest to a vcpkg commit. Without it, a fresh
clone resolves the newest port versions and can break without a code change.

## Verification status

| Path | Status | Evidence |
| --- | --- | --- |
| Windows Release | Passing | `build/windows-release`, `ctest` 1/1 |
| Windows Debug | Not yet re-run after the portability change | — |
| Linux | Passing | CI run [`40279576635`](https://github.com/theobromine7/flux_match/actions/runs/40279576635) |
| Linux local/WSL | Not available | Only the `docker-desktop` WSL distro is installed |

## Known limitations

- Linux is verified in CI, not locally. There is no general-purpose WSL distro
  and free disk space is low, so Docker-based Linux builds are avoided.
- The demo still contains the benchmark and a temporary `__rdtscp` shim
  (`src/main.cpp`). Phase 1 moves the benchmark to `fluxmatch_bench`; Phase 1.8
  replaces the shim with a documented timer module.
- `fluxmatch_marketdata` connects to Binance over TLS and needs network access.
  It is a development tool, not a trading client. The project is simulation-only.
- No Windows CI job yet. It is added in Phase 9, where binary caching for vcpkg
  is worth setting up; without a cache, a Windows runner rebuilds Boost.
- CMake is VS-bundled, so the build environment is coupled to Visual Studio
  updates. A standalone CMake install would remove that coupling.

## Reproduction

See `docs/BUILD.md` for the exact commands. The short version on Windows:

```powershell
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
```

## Re-recording this file

```powershell
cmake --version
git -C $env:VCPKG_ROOT rev-parse HEAD
& "$env:VCPKG_ROOT\vcpkg.exe" version
Get-CimInstance Win32_OperatingSystem | Select-Object Caption, Version, BuildNumber
Get-CimInstance Win32_Processor | Select-Object Name, NumberOfCores, NumberOfLogicalProcessors
Get-PSDrive C | Select-Object Used, Free
Select-String build/windows-release/CMakeFiles/*/CMakeCXXCompiler.cmake `
  -Pattern 'CMAKE_CXX_COMPILER_VERSION'
```
