#pragma once

#include "server_logic/server_logic.hpp"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>

namespace server_logic {

struct ServerLogic::Impl {
    struct PlayerState {
        Player player;
        std::string password_hash;
        std::int32_t running_orders{0};
        std::int32_t done_orders{0};
    };

    struct OrderState {
        OrderInfo info;
        bool cancel_requested{false};
    };

    explicit Impl(ServerConfig config_value);
    ~Impl();

    [[nodiscard]] bool is_valid_login(std::string_view value) const;
    [[nodiscard]] bool is_valid_password(std::string_view value) const;
    [[nodiscard]] static std::string hash_password(std::string_view login, std::string_view password);
    [[nodiscard]] static std::uint64_t now_ms();
    [[nodiscard]] const ItemDefinition& require_item(ItemType item) const;
    [[nodiscard]] PlayerState& require_player_state(std::int64_t player_id);
    [[nodiscard]] const PlayerState& require_player_state(std::int64_t player_id) const;
    [[nodiscard]] OrderState& require_order_state(std::int64_t order_id);
    [[nodiscard]] const OrderState& require_order_state(std::int64_t order_id) const;
    [[nodiscard]] std::vector<RatingEntry> build_rating_locked() const;
    void broadcast_rating_if_changed_locked(std::vector<RatingEntry>& pending_rating, bool force = false);
    void notify_rating_subscribers(std::vector<RatingEntry> rating);
    void worker_loop();
    [[nodiscard]] bool has_runnable_order_locked() const;
    [[nodiscard]] std::optional<std::int64_t> try_start_next_order_locked();
    void execute_order(std::int64_t order_id);

    ServerConfig config;
    mutable std::mutex mutex;
    std::condition_variable queue_cv;
    std::condition_variable order_cv;
    bool stopping{false};
    std::int64_t next_player_id{1};
    std::int64_t next_order_id{1};
    std::size_t next_subscription_id{1};
    std::unordered_map<std::int64_t, PlayerState> players;
    std::unordered_map<std::string, std::int64_t> player_ids_by_login;
    std::unordered_map<std::int64_t, OrderState> orders;
    std::deque<std::int64_t> queued_order_ids;
    std::unordered_map<std::size_t, RatingCallback> rating_subscribers;
    std::vector<RatingEntry> latest_rating;
    std::vector<std::thread> workers;
};

}  // namespace server_logic
