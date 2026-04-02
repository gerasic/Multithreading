#include "test_helpers.hpp"

#include <gtest/gtest.h>

#include <mutex>
#include <set>
#include <thread>
#include <vector>

using server_logic::ItemType;
using server_logic::ServerConfig;
using server_logic::ServerLogic;

TEST(ServerLogicConcurrencyTest, SupportsConcurrentOrderCreation) {
    ServerLogic logic{ServerConfig{.worker_pool_size = 0}};
    const auto player = test_helpers::register_and_login(logic);
    std::vector<std::thread> threads;
    std::mutex mutex;
    std::set<std::int64_t> ids;

    for (int i = 0; i < 8; ++i) {
        threads.emplace_back([&] {
            for (int j = 0; j < 25; ++j) {
                const auto order_id = logic.create_order(player.player_id, ItemType::Gear);
                std::lock_guard lock{mutex};
                ids.insert(order_id);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    const auto orders = logic.list_orders(player.player_id);
    EXPECT_EQ(orders.size(), 200);
    EXPECT_EQ(ids.size(), 200);
}
