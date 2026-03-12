cmake_minimum_required(VERSION 3.30)

project(multithreading_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

find_package(GTest REQUIRED CONFIG)

add_library(server_logic
    src/server_logic_core.cpp
    src/server_logic_utils.cpp
    src/server_logic_auth.cpp
    src/server_logic_orders.cpp
    src/server_logic_rating.cpp
    src/server_logic_execution.cpp
)

target_include_directories(server_logic PUBLIC include)
target_include_directories(server_logic PRIVATE src)

add_executable(server_logic_tests
    tests/auth_tests.cpp
    tests/order_tests.cpp
    tests/execution_tests.cpp
    tests/rating_tests.cpp
    tests/concurrency_tests.cpp
)

target_link_libraries(server_logic_tests
    PRIVATE
        server_logic
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(server_logic_tests)
