cmake_minimum_required(VERSION 3.20)

project(FluxMatch
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost CONFIG REQUIRED COMPONENTS json)
find_package(OpenSSL REQUIRED)

add_executable(flux_match
    src/main.cpp
    src/Order.cpp
    src/OrderBook.cpp
    src/Trade.cpp
    src/MatchingEngine.cpp
)

add_executable(market_data_test
    src/MarketDataClient.cpp
    src/MarketDepth.cpp
)

target_include_directories(flux_match PRIVATE
    include/fluxmatch
)

target_include_directories(market_data_test PRIVATE
    include/fluxmatch
)

target_link_libraries(flux_match PRIVATE
    Boost::boost
)

target_link_libraries(market_data_test PRIVATE
    Boost::boost
    Boost::json
    OpenSSL::SSL
    OpenSSL::Crypto
)

include(CTest)
if(BUILD_TESTING)
    add_executable(order_book_tests
        tests/OrderBookTests.cpp
        src/Order.cpp
        src/OrderBook.cpp
        src/Trade.cpp
        src/MatchingEngine.cpp
    )
    target_include_directories(order_book_tests PRIVATE include/fluxmatch)
    add_test(NAME order_book_tests COMMAND order_book_tests)
endif()
