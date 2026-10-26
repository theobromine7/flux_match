# Build and test FluxMatch

## Windows

### Prerequisites

1. Install **Visual Studio 2022** or **Visual Studio 2022 Build Tools** with the
   **Desktop development with C++** workload, including the MSVC x64 tools and
   Windows SDK.
2. Install **CMake 3.25 or newer** (verified with `3.31.6-msvc6`). You can
   install it from [cmake.org/download](https://cmake.org/download/) or with
   `winget install Kitware.CMake`. Confirm `cmake` is on `PATH` by running
   `cmake --version` in PowerShell. Preset schema version 6 requires CMake 3.25.
3. Install Git for Windows if it is not already installed.
4. Clone and bootstrap vcpkg. The commands below use
   `%USERPROFILE%\Projects\vcpkg`, the vcpkg checkout used to verify this
   project:

   ```powershell
   $vcpkgRoot = "$env:USERPROFILE\Projects\vcpkg"
   if (-not (Test-Path "$vcpkgRoot\scripts\buildsystems\vcpkg.cmake")) {
       New-Item -ItemType Directory -Force (Split-Path $vcpkgRoot) | Out-Null
       git clone https://github.com/microsoft/vcpkg.git $vcpkgRoot
   }
   & "$vcpkgRoot\bootstrap-vcpkg.bat"
   setx VCPKG_ROOT $vcpkgRoot
   ```

   `setx` saves the variable for future processes; it does **not** change the
   environment of the PowerShell window in which it runs. Close that window,
   open a new PowerShell window, and confirm the value before configuring:

   ```powershell
   $env:VCPKG_ROOT
   Test-Path "$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
   ```

   The first CMake configure uses the pinned vcpkg manifest to download and
   build Boost and OpenSSL. Depending on network speed and machine performance,
   allow several minutes to tens of minutes and ensure internet access is
   available. This verification machine had a populated vcpkg binary cache; a
   clean machine may need to build the dependency graph from source.

### Configure, build, and test

Run these commands in the new PowerShell window:

```powershell
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
```

The Debug configuration uses the same steps with `windows-debug` instead.
Visual Studio presets are multi-configuration: select Debug or Release with the
preset; `CMAKE_BUILD_TYPE` is not used. Executables are under
`build\windows-release\Release\` (or `build\windows-debug\Debug\`):

- `fluxmatch_demo.exe`
- `fluxmatch_marketdata.exe`
- `fluxmatch_tests.exe`

CTest runs `fluxmatch_tests`; `ctest --preset windows-release` shows failures
and their output.

### Troubleshooting

- **`VCPKG_ROOT` is empty:** `setx` was run in another process, or the terminal
  was not reopened. Open a new PowerShell window and check `$env:VCPKG_ROOT`.
- **The vcpkg toolchain cannot be found:** check that
  `"$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"` exists. The preset uses
  this path, so an empty or incorrect variable makes the toolchain path
  invalid.
- **CMake reports stale or incompatible cached settings:** remove only the
  affected preset's build directory (for example,
  `Remove-Item -Recurse -Force build\windows-release`) and configure again.
  CMake caches the toolchain and compiler on first configure.
- **A dependency download or build fails:** check network access, available
  disk space, and the vcpkg output. Re-run configure after resolving the
  reported error.

## Linux (including WSL)

The Windows presets use Visual Studio and vcpkg. On Linux, install system
dependencies and configure directly; CMake does not require a vcpkg toolchain:

```bash
sudo apt update
sudo apt install build-essential cmake libboost-all-dev libssl-dev
cmake -S . -B build/linux-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux-release --parallel
ctest --test-dir build/linux-release --output-on-failure
```

To build only the matching engine, demo, and tests without market-data
dependencies, configure with `-DFLUXMATCH_BUILD_MARKET_DATA=OFF`. This avoids
needing Boost and OpenSSL entirely:

```bash
cmake -S . -B build/linux-core -DCMAKE_BUILD_TYPE=Release \
  -DFLUXMATCH_BUILD_MARKET_DATA=OFF
```

## Continuous integration

`.github/workflows/cpp.yml` builds and tests the project on a clean
`ubuntu-latest` runner. It is the authoritative Linux check: it starts with no
cached build directory, no pre-installed Boost/OpenSSL, and no `VCPKG_ROOT`, so
a green run proves the repository is self-sufficient on Linux. Windows CI is
added in Phase 9; the Windows path is currently verified locally with the
presets above.

## Current limitations

- The checked-in configure/build/test presets target Windows with Visual Studio
  2022 and vcpkg. Linux/WSL uses the direct commands above; it does not yet have
  a dedicated preset or CI job.
- The benchmark and market-data client are development tools, not production
  trading infrastructure. The market-data executable connects to Binance and
  requires network access.
- Build and test success do not establish exchange-grade latency or operational
  readiness.
