#include "test_helpers.hpp"

#include <gtest/gtest.h>

using server_logic::ErrorCode;
using server_logic::ItemType;
using server_logic::OrderStatus;
using server_logic::ServerConfig;
using server_logic::ServerError;
using server_logic::ServerLogic;

TEST(ServerLogicOrdersTest, CreatesQueuedOrderAndReturnsItInList) {
    ServerLogic logic{ServerConfig{.worker_pool_size = 0}};
    const auto player = test_helpers::register_and_login(logic);

    const auto order_id = logic.create_order(player.player_id, ItemType::Gear);
    const auto orders = logic.list_orders(player.player_id);

    ASSERT_EQ(orders.size(), 1);
    EXPECT_EQ(order_id, orders.front().order_id);
    EXPECT_EQ(orders.front().player_id, player.player_id);
    EXPECT_EQ(orders.front().item, ItemType::Gear);
    EXPECT_EQ(orders.front().status, OrderStatus::Queued);
    EXPECT_EQ(orders.front().progress, 0);
    EXPECT_EQ(orders.front().reward_coins_total, 2);
}

TEST(ServerLogicOrdersTest, ReturnsAggregatedStatsForPlayerOrders) {
    ServerLogic logic{ServerConfig{.worker_pool_size = 0}};
    const auto player = test_helpers::register_and_login(logic);

    static_cast<void>(logic.create_order(player.player_id, ItemType::Gear));
    static_cast<void>(logic.create_order(player.player_id, ItemType::Plate));

    const auto stats = logic.get_stats(player.player_id);

    EXPECT_EQ(stats.coins, 20);
    EXPECT_EQ(stats.machines, 1);
    EXPECT_EQ(stats.queued, 2);
    EXPECT_EQ(stats.running, 0);
    EXPECT_EQ(stats.done, 0);
    EXPECT_EQ(stats.failed, 0);
}

TEST(ServerLogicOrdersTest, RejectsOrderCreationForUnknownPlayer) {
    ServerLogic logic{ServerConfig{.worker_pool_size = 0}};

    try {
        static_cast<void>(logic.create_order(42, ItemType::Gear));
        FAIL();
    } catch (const ServerError& error) {
        EXPECT_EQ(error.code(), ErrorCode::UnknownPlayer);
    }
}

TEST(ServerLogicOrdersTest, RejectsOrderWhenQueueIsFull) {
    ServerConfig config;
    config.worker_pool_size = 0;
    config.max_queue_size = 1;
    ServerLogic logic{config};
    const auto player = test_helpers::register_and_login(logic);

    static_cast<void>(logic.create_order(player.player_id, ItemType::Gear));

    try {
        static_cast<void>(logic.create_order(player.player_id, ItemType::Plate));
        FAIL();
    } catch (const ServerError& error) {
        EXPECT_EQ(error.code(), ErrorCode::QueueFull);
    }
}
