cmake_minimum_required(VERSION 3.25)

project(FluxMatch
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(fluxmatch_core
    src/Order.cpp
    src/OrderBook.cpp
    src/Trade.cpp
    src/MatchingEngine.cpp
)

target_include_directories(fluxmatch_core PUBLIC
    include
)

add_executable(fluxmatch_demo
    src/main.cpp
)

target_link_libraries(fluxmatch_demo PRIVATE
    fluxmatch_core
)

add_library(fluxmatch_workloads
    benchmarks/workloads.cpp
)

target_include_directories(fluxmatch_workloads PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks
)

target_link_libraries(fluxmatch_workloads PUBLIC
    fluxmatch_core
)

add_executable(fluxmatch_bench
    benchmarks/bench_main.cpp
)

target_link_libraries(fluxmatch_bench PRIVATE
    fluxmatch_workloads
)

# Record the facts needed to interpret a benchmark result: which source
# revision, which compiler, which build type, and which platform.
find_package(Git QUIET)
set(FLUXMATCH_GIT_COMMIT "unknown")
if(Git_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE FLUXMATCH_GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()

target_compile_definitions(fluxmatch_bench PRIVATE
    FLUXMATCH_GIT_COMMIT="${FLUXMATCH_GIT_COMMIT}"
    FLUXMATCH_COMPILER_ID="${CMAKE_CXX_COMPILER_ID}"
    FLUXMATCH_COMPILER_VERSION="${CMAKE_CXX_COMPILER_VERSION}"
    FLUXMATCH_CXX_STANDARD="${CMAKE_CXX_STANDARD}"
    FLUXMATCH_SYSTEM_NAME="${CMAKE_SYSTEM_NAME}"
    FLUXMATCH_SYSTEM_PROCESSOR="${CMAKE_SYSTEM_PROCESSOR}"
    FLUXMATCH_BUILD_CONFIG="$<CONFIG>"
)

option(FLUXMATCH_BUILD_MARKET_DATA "Build the market-data client" ON)

if(FLUXMATCH_BUILD_MARKET_DATA)
    find_package(Boost CONFIG REQUIRED COMPONENTS json)
    find_package(OpenSSL REQUIRED)

    add_executable(fluxmatch_marketdata
        src/MarketDataClient.cpp
        src/MarketDepth.cpp
    )

    target_include_directories(fluxmatch_marketdata PRIVATE
        include
    )

    target_link_libraries(fluxmatch_marketdata PRIVATE
        Boost::json
        OpenSSL::SSL
        OpenSSL::Crypto
    )
endif()

include(CTest)

if(BUILD_TESTING)
    add_executable(fluxmatch_tests
        tests/OrderBookTests.cpp
    )

    target_link_libraries(fluxmatch_tests PRIVATE
        fluxmatch_core
    )

    add_test(NAME fluxmatch_tests COMMAND fluxmatch_tests)

    add_executable(fluxmatch_workload_tests
        tests/WorkloadTests.cpp
    )

    target_link_libraries(fluxmatch_workload_tests PRIVATE
        fluxmatch_workloads
    )

    add_test(NAME fluxmatch_workload_tests COMMAND fluxmatch_workload_tests)
endif()
