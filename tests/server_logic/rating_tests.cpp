#include "test_helpers.hpp"

#include <gtest/gtest.h>

#include <mutex>
#include <vector>

using server_logic::ItemType;
using server_logic::RatingEntry;
using server_logic::ServerLogic;

TEST(ServerLogicRatingTest, RecalculatesRatingAfterOrderCompletion) {
    ServerLogic logic{test_helpers::make_async_config()};
    const auto first = test_helpers::register_and_login(logic, "alex", "qwerty123");
    const auto second = test_helpers::register_and_login(logic, "max", "qwerty123");

    const auto order_id = logic.create_order(second.player_id, ItemType::Circuit);
    ASSERT_TRUE(logic.wait_for_order_terminal_state(order_id, std::chrono::milliseconds{1000}));

    const auto rating = logic.get_rating(first.player_id);

    ASSERT_EQ(rating.size(), 2);
    EXPECT_EQ(rating[0].player_id, second.player_id);
    EXPECT_EQ(rating[0].login, "max");
    EXPECT_EQ(rating[0].done_orders, 1);
    EXPECT_EQ(rating[0].rating_score, 35);
}

TEST(ServerLogicRatingTest, PublishesLiveRatingUpdates) {
    ServerLogic logic{test_helpers::make_async_config()};
    const auto first = test_helpers::register_and_login(logic, "alex", "qwerty123");
    const auto second = test_helpers::register_and_login(logic, "max", "qwerty123");
    std::mutex mutex;
    std::vector<std::vector<RatingEntry>> updates;

    const auto subscription = logic.subscribe_rating_updates([&](std::vector<RatingEntry> rating) {
        std::lock_guard lock{mutex};
        updates.push_back(std::move(rating));
    });

    const auto order_id = logic.create_order(second.player_id, ItemType::Circuit);
    ASSERT_TRUE(logic.wait_for_order_terminal_state(order_id, std::chrono::milliseconds{1000}));
    ASSERT_TRUE(test_helpers::wait_until(
        [&] {
            std::lock_guard lock{mutex};
            return updates.size() >= 2;
        },
        std::chrono::milliseconds{500}));

    logic.unsubscribe_rating_updates(subscription);

    std::lock_guard lock{mutex};
    ASSERT_FALSE(updates.empty());
    EXPECT_EQ(updates.back()[0].player_id, second.player_id);
    EXPECT_EQ(updates.back()[0].login, "max");
    EXPECT_EQ(updates.back()[0].done_orders, 1);
}
