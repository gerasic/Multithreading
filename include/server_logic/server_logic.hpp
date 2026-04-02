#pragma once

#include "server_logic/error.hpp"
#include "server_logic/types.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace server_logic {

class ServerLogic {
public:
    explicit ServerLogic(ServerConfig config = {});
    ~ServerLogic();

    ServerLogic(const ServerLogic&) = delete;
    ServerLogic& operator=(const ServerLogic&) = delete;

    [[nodiscard]] AuthResult register_user(const std::string& login, const std::string& password);
    [[nodiscard]] AuthResult login(const std::string& login, const std::string& password) const;
    [[nodiscard]] Player get_player(std::int64_t player_id) const;
    [[nodiscard]] std::int64_t create_order(std::int64_t player_id, ItemType item);
    [[nodiscard]] std::vector<OrderInfo> list_orders(std::int64_t player_id) const;
    [[nodiscard]] StatsSnapshot get_stats(std::int64_t player_id) const;
    [[nodiscard]] BuyMachineResult buy_machine(std::int64_t player_id);
    void cancel_order(std::int64_t player_id, std::int64_t order_id);
    [[nodiscard]] std::vector<RatingEntry> get_rating(std::int64_t player_id) const;
    [[nodiscard]] std::size_t subscribe_rating_updates(RatingCallback callback);
    void unsubscribe_rating_updates(std::size_t subscription_id);
    [[nodiscard]] bool wait_for_order_terminal_state(std::int64_t order_id, std::chrono::milliseconds timeout);

private:
    struct Impl;
    Impl* impl_;
};

}  // namespace server_logic
