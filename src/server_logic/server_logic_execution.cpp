#include "server_logic_impl.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

namespace server_logic {

void ServerLogic::Impl::worker_loop() {
    while (true) {
        std::optional<std::int64_t> order_id;
        {
            std::unique_lock lock{mutex};
            queue_cv.wait(lock, [&] {
                return stopping || has_runnable_order_locked();
            });
            if (stopping) {
                return;
            }
            order_id = try_start_next_order_locked();
        }
        if (order_id.has_value()) {
            execute_order(*order_id);
        }
    }
}

bool ServerLogic::Impl::has_runnable_order_locked() const {
    for (const auto order_id : queued_order_ids) {
        const auto& order = require_order_state(order_id);
        const auto& player = require_player_state(order.info.player_id);
        if (order.info.status == OrderStatus::Queued && player.running_orders < player.player.machines) {
            return true;
        }
    }
    return false;
}

std::optional<std::int64_t> ServerLogic::Impl::try_start_next_order_locked() {
    for (auto it = queued_order_ids.begin(); it != queued_order_ids.end(); ++it) {
        auto& order = require_order_state(*it);
        auto& player = require_player_state(order.info.player_id);
        if (order.info.status != OrderStatus::Queued) {
            it = queued_order_ids.erase(it);
            if (it == queued_order_ids.end()) {
                return std::nullopt;
            }
            --it;
            continue;
        }
        if (player.running_orders >= player.player.machines) {
            continue;
        }
        order.info.status = OrderStatus::Running;
        order.info.started_at = now_ms();
        ++player.running_orders;
        const auto order_id = *it;
        queued_order_ids.erase(it);
        return order_id;
    }
    return std::nullopt;
}

void ServerLogic::Impl::execute_order(const std::int64_t order_id) {
    ItemDefinition definition{};
    {
        std::lock_guard lock{mutex};
        const auto& order = require_order_state(order_id);
        definition = require_item(order.info.item);
    }

    const auto total_ms = definition.base_time_ms;
    const auto tick_ms = std::max<std::int64_t>(1, config.tick_ms);
    const auto minimum_steps = std::int64_t{5};
    const auto natural_steps = std::max<std::int64_t>(1, static_cast<std::int64_t>(std::ceil(static_cast<long double>(total_ms) / tick_ms)));
    const auto steps = std::max(minimum_steps, natural_steps);
    const auto sleep_per_step = std::max<std::int64_t>(1, static_cast<std::int64_t>(std::ceil(static_cast<long double>(total_ms) / steps)));

    for (std::int64_t step = 1; step <= steps; ++step) {
        std::this_thread::sleep_for(std::chrono::milliseconds{sleep_per_step});

        bool cancelled = false;
        std::vector<RatingEntry> pending_rating;
        {
            std::lock_guard lock{mutex};
            auto& order = require_order_state(order_id);
            auto& player = require_player_state(order.info.player_id);

            if (order.cancel_requested) {
                order.info.status = OrderStatus::Cancelled;
                order.info.finished_at = now_ms();
                order.info.progress = std::min(order.info.progress, 99);
                --player.running_orders;
                cancelled = true;
            } else {
                const auto progress = static_cast<std::int32_t>(std::min<std::int64_t>(99, (step * 100) / steps));
                order.info.progress = std::max(order.info.progress, progress);
            }

            if (cancelled) {
                order_cv.notify_all();
            } else if (step == steps) {
                order.info.status = OrderStatus::Done;
                order.info.progress = 100;
                order.info.finished_at = now_ms();
                player.player.coins += order.info.reward_coins_total;
                --player.running_orders;
                ++player.done_orders;
                broadcast_rating_if_changed_locked(pending_rating);
                order_cv.notify_all();
            }
        }
        if (!pending_rating.empty()) {
            notify_rating_subscribers(std::move(pending_rating));
        }
        if (cancelled) {
            queue_cv.notify_all();
            return;
        }
    }
    queue_cv.notify_all();
}

}  // namespace server_logic
