#include "server_logic_impl.hpp"

#include <mutex>
#include <ranges>
#include <utility>

namespace server_logic {

ServerLogic::ServerLogic(ServerConfig config) : impl_(new Impl(std::move(config))) {
    if (impl_->config.start_machines < 1) {
        impl_->config.start_machines = 1;
    }
    if (impl_->config.tick_ms < 1) {
        impl_->config.tick_ms = 1;
    }
    if (impl_->config.min_password_length < 1) {
        impl_->config.min_password_length = 1;
    }
    if (impl_->config.max_password_length < impl_->config.min_password_length) {
        impl_->config.max_password_length = impl_->config.min_password_length;
    }

    for (std::size_t i = 0; i < impl_->config.worker_pool_size; ++i) {
        impl_->workers.emplace_back(&Impl::worker_loop, impl_);
    }
}

ServerLogic::~ServerLogic() {
    {
        std::lock_guard lock{impl_->mutex};
        impl_->stopping = true;
    }
    impl_->queue_cv.notify_all();
    impl_->order_cv.notify_all();
    for (auto& worker : impl_->workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    delete impl_;
}

Player ServerLogic::get_player(std::int64_t player_id) const {
    std::lock_guard lock{impl_->mutex};
    return impl_->require_player_state(player_id).player;
}

bool ServerLogic::wait_for_order_terminal_state(std::int64_t order_id, std::chrono::milliseconds timeout) {
    std::unique_lock lock{impl_->mutex};
    return impl_->order_cv.wait_for(lock, timeout, [&] {
        const auto it = impl_->orders.find(order_id);
        if (it == impl_->orders.end()) {
            throw ServerError{ErrorCode::UnknownOrder, "Order not found"};
        }
        const auto status = it->second.info.status;
        return status == OrderStatus::Done || status == OrderStatus::Failed || status == OrderStatus::Cancelled;
    });
}

const ItemDefinition& ServerLogic::Impl::require_item(ItemType item) const {
    const auto it = std::ranges::find_if(config.items, [item](const ItemDefinition& definition) {
        return definition.item == item;
    });
    if (it == config.items.end()) {
        throw ServerError{ErrorCode::InvalidItem, "Unknown item"};
    }
    return *it;
}

ServerLogic::Impl::PlayerState& ServerLogic::Impl::require_player_state(std::int64_t player_id) {
    const auto it = players.find(player_id);
    if (it == players.end()) {
        throw ServerError{ErrorCode::UnknownPlayer, "Player not found"};
    }
    return it->second;
}

const ServerLogic::Impl::PlayerState& ServerLogic::Impl::require_player_state(std::int64_t player_id) const {
    const auto it = players.find(player_id);
    if (it == players.end()) {
        throw ServerError{ErrorCode::UnknownPlayer, "Player not found"};
    }
    return it->second;
}

ServerLogic::Impl::OrderState& ServerLogic::Impl::require_order_state(std::int64_t order_id) {
    const auto it = orders.find(order_id);
    if (it == orders.end()) {
        throw ServerError{ErrorCode::UnknownOrder, "Order not found"};
    }
    return it->second;
}

const ServerLogic::Impl::OrderState& ServerLogic::Impl::require_order_state(std::int64_t order_id) const {
    const auto it = orders.find(order_id);
    if (it == orders.end()) {
        throw ServerError{ErrorCode::UnknownOrder, "Order not found"};
    }
    return it->second;
}

}  // namespace server_logic
