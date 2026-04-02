#include "api_server/tcp_server.hpp"
#include "server_logic/server_logic.hpp"

#include <boost/asio/io_context.hpp>

#include <thread>
#include <vector>

int main() {
    constexpr unsigned short port = 8080;
    constexpr int io_threads = 2;

    boost::asio::io_context io_context;
    server_logic::ServerLogic logic;
    api_server::TcpServer server{io_context, logic, port};
    server.start();

    std::vector<std::thread> threads;
    threads.reserve(io_threads);
    for (int index = 0; index < io_threads; ++index) {
        threads.emplace_back([&io_context] {
            io_context.run();
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    return 0;
}
