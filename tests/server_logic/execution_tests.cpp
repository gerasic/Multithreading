#include "test_helpers.hpp"

#include <gtest/gtest.h>

#include <vector>

using server_logic::ErrorCode;
using server_logic::ItemType;
using server_logic::OrderStatus;
using server_logic::ServerConfig;
using server_logic::ServerError;
using server_logic::ServerLogic;

TEST(ServerLogicExecutionTest, CompletesOrderAndAwardsCoins) {
    ServerLogic logic{test_helpers::make_async_config()};
    const auto player = test_helpers::register_and_login(logic);

    const auto order_id = logic.create_order(player.player_id, ItemType::Gear);

    ASSERT_TRUE(logic.wait_for_order_terminal_state(order_id, std::chrono::milliseconds{500}));
    const auto orders = logic.list_orders(player.player_id);
    const auto stats = logic.get_stats(player.player_id);

    ASSERT_EQ(orders.size(), 1);
    EXPECT_EQ(orders.front().status, OrderStatus::Done);
    EXPECT_EQ(orders.front().progress, 100);
    EXPECT_EQ(stats.done, 1);
    EXPECT_EQ(stats.coins, 22);
}

TEST(ServerLogicExecutionTest, UpdatesProgressMonotonicallyWhileOrderIsRunning) {
    ServerLogic logic{test_helpers::make_async_config()};
    const auto player = test_helpers::register_and_login(logic);

    static_cast<void>(logic.create_order(player.player_id, ItemType::Circuit));
    std::vector<int> progress_values;

    ASSERT_TRUE(test_helpers::wait_until(
        [&] {
            const auto orders = logic.list_orders(player.player_id);
            if (orders.empty()) {
                return false;
            }
            progress_values.push_back(orders.front().progress);
            return orders.front().status == OrderStatus::Done;
        },
        std::chrono::milliseconds{1500}));

    ASSERT_GE(progress_values.size(), 5U);
    for (std::size_t i = 1; i < progress_values.size(); ++i) {
        EXPECT_GE(progress_values[i], progress_values[i - 1]);
    }
}

TEST(ServerLogicExecutionTest, RespectsMachineLimitAndKeepsExtraOrdersQueued) {
    ServerLogic logic{test_helpers::make_async_config()};
    const auto player = test_helpers::register_and_login(logic);

    const auto first = logic.create_order(player.player_id, ItemType::Circuit);
    const auto second = logic.create_order(player.player_id, ItemType::Circuit);

    ASSERT_TRUE(test_helpers::wait_until(
        [&] {
            const auto orders = logic.list_orders(player.player_id);
            if (orders.size() != 2) {
                return false;
            }
            return orders[0].status == OrderStatus::Running && orders[1].status == OrderStatus::Queued;
        },
        std::chrono::milliseconds{200}));

    ASSERT_TRUE(logic.wait_for_order_terminal_state(first, std::chrono::milliseconds{1000}));
    ASSERT_TRUE(logic.wait_for_order_terminal_state(second, std::chrono::milliseconds{1000}));
}

TEST(ServerLogicBuyMachineTest, BuysMachineWhenEnoughCoinsAvailable) {
    auto config = test_helpers::make_async_config();
    config.start_coins = 40;
    ServerLogic logic{config};
    const auto player = test_helpers::register_and_login(logic);

    const auto result = logic.buy_machine(player.player_id);

    EXPECT_EQ(result.machines, 2);
    EXPECT_EQ(result.coins, 25);
    EXPECT_EQ(logic.get_stats(player.player_id).machines, 2);
}

TEST(ServerLogicBuyMachineTest, RejectsPurchaseWhenCoinsAreInsufficient) {
    auto config = test_helpers::make_async_config();
    config.start_coins = 10;
    ServerLogic logic{config};
    const auto player = test_helpers::register_and_login(logic);

    try {
        static_cast<void>(logic.buy_machine(player.player_id));
        FAIL();
    } catch (const ServerError& error) {
        EXPECT_EQ(error.code(), ErrorCode::NotEnoughCoins);
    }
}

TEST(ServerLogicCancelOrderTest, CancelsQueuedOrder) {
    ServerConfig config = test_helpers::make_async_config();
    config.worker_pool_size = 0;
    ServerLogic logic{config};
    const auto player = test_helpers::register_and_login(logic);
    const auto order_id = logic.create_order(player.player_id, ItemType::Gear);

    logic.cancel_order(player.player_id, order_id);

    const auto orders = logic.list_orders(player.player_id);
    ASSERT_EQ(orders.size(), 1);
    EXPECT_EQ(orders.front().status, OrderStatus::Cancelled);
}

TEST(ServerLogicCancelOrderTest, CancelsRunningOrderCooperatively) {
    ServerLogic logic{test_helpers::make_async_config()};
    const auto player = test_helpers::register_and_login(logic);
    const auto order_id = logic.create_order(player.player_id, ItemType::Circuit);

    ASSERT_TRUE(test_helpers::wait_until(
        [&] {
            const auto orders = logic.list_orders(player.player_id);
            return !orders.empty() && orders.front().status == OrderStatus::Running;
        },
        std::chrono::milliseconds{200}));

    logic.cancel_order(player.player_id, order_id);

    ASSERT_TRUE(logic.wait_for_order_terminal_state(order_id, std::chrono::milliseconds{1000}));
    const auto orders = logic.list_orders(player.player_id);
    ASSERT_EQ(orders.size(), 1);
    EXPECT_EQ(orders.front().status, OrderStatus::Cancelled);
}
