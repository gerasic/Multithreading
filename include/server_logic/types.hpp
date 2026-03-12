#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace server_logic {

enum class ItemType {
    Gear,
    Plate,
    Circuit
};

enum class OrderStatus {
    Queued,
    Running,
    Done,
    Failed,
    Cancelled
};

struct ItemDefinition {
    ItemType item;
    std::int64_t base_time_ms;
    std::int64_t reward_coins;

    auto operator<=>(const ItemDefinition&) const = default;
};

struct ServerConfig {
    std::int64_t start_coins{20};
    std::int32_t start_machines{1};
    std::size_t max_login_length{32};
    std::size_t min_password_length{8};
    std::size_t max_password_length{128};
    std::size_t max_queue_size{1024};
    std::size_t worker_pool_size{4};
    std::int64_t tick_ms{50};
    std::int64_t machine_cost{15};
    std::int64_t rating_done_order_bonus{10};
    std::vector<ItemDefinition> items{
        ItemDefinition{ItemType::Gear, 120, 2},
        ItemDefinition{ItemType::Plate, 180, 3},
        ItemDefinition{ItemType::Circuit, 250, 5},
    };
};

struct Player {
    std::int64_t player_id{};
    std::string login;
    std::int64_t coins{};
    std::int32_t machines{};

    auto operator<=>(const Player&) const = default;
};

struct AuthResult {
    std::int64_t player_id{};
    std::string login;
    std::int64_t coins{};
    std::int32_t machines{};

    auto operator<=>(const AuthResult&) const = default;
};

struct OrderInfo {
    std::int64_t order_id{};
    std::int64_t player_id{};
    ItemType item{};
    OrderStatus status{};
    std::int32_t progress{};
    std::uint64_t created_at{};
    std::optional<std::uint64_t> started_at;
    std::optional<std::uint64_t> finished_at;
    std::int64_t reward_coins_total{};

    auto operator<=>(const OrderInfo&) const = default;
};

struct StatsSnapshot {
    std::int64_t coins{};
    std::int32_t machines{};
    std::int32_t queued{};
    std::int32_t running{};
    std::int32_t done{};
    std::int32_t failed{};

    auto operator<=>(const StatsSnapshot&) const = default;
};

struct BuyMachineResult {
    std::int32_t machines{};
    std::int64_t coins{};

    auto operator<=>(const BuyMachineResult&) const = default;
};

struct RatingEntry {
    std::int32_t place{};
    std::int64_t player_id{};
    std::string login;
    std::int64_t coins{};
    std::int32_t done_orders{};
    std::int64_t rating_score{};

    auto operator<=>(const RatingEntry&) const = default;
};

using RatingCallback = std::function<void(std::vector<RatingEntry>)>;

}  // namespace server_logic
