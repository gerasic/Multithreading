cmake_minimum_required(VERSION 3.30)

project(multithreading_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

find_package(Boost REQUIRED CONFIG COMPONENTS system json)
find_package(GTest REQUIRED CONFIG)

add_library(server_logic
    src/server_logic/server_logic_core.cpp
    src/server_logic/server_logic_utils.cpp
    src/server_logic/server_logic_auth.cpp
    src/server_logic/server_logic_orders.cpp
    src/server_logic/server_logic_rating.cpp
    src/server_logic/server_logic_execution.cpp
)

target_include_directories(server_logic PUBLIC include)
target_include_directories(server_logic PRIVATE src/server_logic)

add_library(api_server
    src/api_server/client_session.cpp
    src/api_server/json_protocol.cpp
    src/api_server/tcp_server.cpp
)

target_include_directories(api_server PUBLIC include)
target_link_libraries(api_server PUBLIC server_logic)
target_link_libraries(api_server PUBLIC Boost::system Boost::json)

add_executable(api_server_app
    src/app/main.cpp
)

target_link_libraries(api_server_app
    PRIVATE
        api_server
)

add_executable(server_logic_tests
    tests/server_logic/auth_tests.cpp
    tests/server_logic/order_tests.cpp
    tests/server_logic/execution_tests.cpp
    tests/server_logic/rating_tests.cpp
    tests/server_logic/concurrency_tests.cpp
)

target_include_directories(server_logic_tests PRIVATE tests/server_logic)

target_link_libraries(server_logic_tests
    PRIVATE
        server_logic
        GTest::gtest_main
)

add_executable(api_server_tests
    tests/api_server/json_protocol_tests.cpp
    tests/api_server/tcp_server_tests.cpp
)

target_include_directories(api_server_tests PRIVATE tests/server_logic)

target_link_libraries(api_server_tests
    PRIVATE
        api_server
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(server_logic_tests)
gtest_discover_tests(api_server_tests)
