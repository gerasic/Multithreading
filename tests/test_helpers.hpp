#pragma once

#include "server_logic/server_logic.hpp"

#include <chrono>
#include <thread>

namespace test_helpers {

inline server_logic::ServerConfig make_async_config() {
    server_logic::ServerConfig config;
    config.worker_pool_size = 2;
    config.tick_ms = 5;
    config.machine_cost = 15;
    config.rating_done_order_bonus = 10;
    config.items = {
        {server_logic::ItemType::Gear, 25, 2},
        {server_logic::ItemType::Plate, 35, 3},
        {server_logic::ItemType::Circuit, 45, 5},
    };
    return config;
}

template <typename Predicate>
bool wait_until(Predicate&& predicate, const std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }
    return predicate();
}

inline server_logic::AuthResult register_and_login(
    server_logic::ServerLogic& logic,
    const std::string& login = "alex",
    const std::string& password = "qwerty123") {
    static_cast<void>(logic.register_user(login, password));
    return logic.login(login, password);
}

}  // namespace test_helpers
