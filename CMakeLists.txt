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

add_executable(fluxmatch_bench
    benchmarks/bench_main.cpp
)

target_link_libraries(fluxmatch_bench PRIVATE
    fluxmatch_core
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
endif()
