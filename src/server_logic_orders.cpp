#include "server_logic_impl.hpp"

#include <algorithm>

namespace server_logic {

std::int64_t ServerLogic::create_order(std::int64_t player_id, ItemType item) {
    const auto& definition = impl_->require_item(item);

    std::int64_t order_id{};
    {
        std::lock_guard lock{impl_->mutex};
        static_cast<void>(impl_->require_player_state(player_id));
        if (impl_->queued_order_ids.size() >= impl_->config.max_queue_size) {
            throw ServerError{ErrorCode::QueueFull, "Production queue is full"};
        }

        order_id = impl_->next_order_id++;
        auto [it, inserted] = impl_->orders.emplace(order_id, Impl::OrderState{
            .info =
                OrderInfo{
                    .order_id = order_id,
                    .player_id = player_id,
                    .item = item,
                    .status = OrderStatus::Queued,
                    .progress = 0,
                    .created_at = Impl::now_ms(),
                    .started_at = std::nullopt,
                    .finished_at = std::nullopt,
                    .reward_coins_total = definition.reward_coins,
                },
        });
        if (!inserted) {
            throw std::logic_error{"Failed to create order"};
        }
        impl_->queued_order_ids.push_back(order_id);
    }
    impl_->queue_cv.notify_all();
    return order_id;
}

std::vector<OrderInfo> ServerLogic::list_orders(std::int64_t player_id) const {
    std::lock_guard lock{impl_->mutex};
    static_cast<void>(impl_->require_player_state(player_id));

    std::vector<OrderInfo> result;
    result.reserve(impl_->orders.size());
    for (const auto& [order_id, order] : impl_->orders) {
        if (order.info.player_id == player_id) {
            result.push_back(order.info);
        }
    }
    std::ranges::sort(result, [](const OrderInfo& lhs, const OrderInfo& rhs) {
        return lhs.order_id < rhs.order_id;
    });
    return result;
}

StatsSnapshot ServerLogic::get_stats(std::int64_t player_id) const {
    std::lock_guard lock{impl_->mutex}; // shared
    const auto& player = impl_->require_player_state(player_id);

    StatsSnapshot stats{
        .coins = player.player.coins,
        .machines = player.player.machines,
    };

    for (const auto& [order_id, order] : impl_->orders) {
        if (order.info.player_id != player_id) {
            continue;
        }
        switch (order.info.status) {
        case OrderStatus::Queued:
            ++stats.queued;
            break;
        case OrderStatus::Running:
            ++stats.running;
            break;
        case OrderStatus::Done:
            ++stats.done;
            break;
        case OrderStatus::Failed:
            ++stats.failed;
            break;
        case OrderStatus::Cancelled:
            break;
        }
    }

    return stats;
}

BuyMachineResult ServerLogic::buy_machine(std::int64_t player_id) {
    std::vector<RatingEntry> pending_rating;
    BuyMachineResult result;
    {
        std::lock_guard lock{impl_->mutex};
        auto& player = impl_->require_player_state(player_id);
        if (player.player.coins < impl_->config.machine_cost) {
            throw ServerError{ErrorCode::NotEnoughCoins, "Not enough coins"};
        }
        player.player.coins -= impl_->config.machine_cost;
        ++player.player.machines;
        result = BuyMachineResult{
            .machines = player.player.machines,
            .coins = player.player.coins,
        };
        impl_->broadcast_rating_if_changed_locked(pending_rating);
    }
    impl_->queue_cv.notify_all();
    if (!pending_rating.empty()) {
        impl_->notify_rating_subscribers(std::move(pending_rating));
    }
    return result;
}

void ServerLogic::cancel_order(std::int64_t player_id, std::int64_t order_id) {
    {
        std::lock_guard lock{impl_->mutex};
        static_cast<void>(impl_->require_player_state(player_id));
        auto& order = impl_->require_order_state(order_id);
        if (order.info.player_id != player_id) {
            throw ServerError{ErrorCode::Forbidden, "Order belongs to another player"};
        }
        if (order.info.status == OrderStatus::Done || order.info.status == OrderStatus::Failed ||
            order.info.status == OrderStatus::Cancelled) {
            throw ServerError{ErrorCode::CannotCancel, "Order cannot be cancelled"};
        }
        if (order.info.status == OrderStatus::Queued) {
            order.info.status = OrderStatus::Cancelled;
            order.info.finished_at = Impl::now_ms();
            const auto it = std::find(impl_->queued_order_ids.begin(), impl_->queued_order_ids.end(), order_id);
            if (it != impl_->queued_order_ids.end()) {
                impl_->queued_order_ids.erase(it);
            }
        } else {
            order.cancel_requested = true;
        }
    }
    impl_->queue_cv.notify_all();
    impl_->order_cv.notify_all();
}

}  // namespace server_logic
