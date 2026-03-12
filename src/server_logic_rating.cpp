#include "server_logic_impl.hpp"

#include <algorithm>
#include <stdexcept>

namespace server_logic {

std::vector<RatingEntry> ServerLogic::get_rating(std::int64_t player_id) const {
    std::lock_guard lock{impl_->mutex};
    static_cast<void>(impl_->require_player_state(player_id));
    return impl_->build_rating_locked();
}

std::size_t ServerLogic::subscribe_rating_updates(RatingCallback callback) {
    if (!callback) {
        throw std::invalid_argument{"Rating callback must be valid"};
    }

    std::vector<RatingEntry> snapshot;
    std::size_t subscription_id{};
    {
        std::lock_guard lock{impl_->mutex};
        subscription_id = impl_->next_subscription_id++;
        impl_->rating_subscribers.emplace(subscription_id, callback);
        snapshot = impl_->latest_rating;
    }
    callback(std::move(snapshot));
    return subscription_id;
}

void ServerLogic::unsubscribe_rating_updates(std::size_t subscription_id) {
    std::lock_guard lock{impl_->mutex};
    impl_->rating_subscribers.erase(subscription_id);
}

std::vector<RatingEntry> ServerLogic::Impl::build_rating_locked() const {
    std::vector<RatingEntry> rating;
    rating.reserve(players.size());

    for (const auto& [player_id, player_state] : players) {
        rating.push_back(RatingEntry{
            .player_id = player_id,
            .login = player_state.player.login,
            .coins = player_state.player.coins,
            .done_orders = player_state.done_orders,
            .rating_score = player_state.player.coins + player_state.done_orders * config.rating_done_order_bonus,
        });
    }

    std::ranges::sort(rating, [](const RatingEntry& lhs, const RatingEntry& rhs) {
        if (lhs.rating_score != rhs.rating_score) {
            return lhs.rating_score > rhs.rating_score;
        }
        if (lhs.coins != rhs.coins) {
            return lhs.coins > rhs.coins;
        }
        return lhs.player_id < rhs.player_id;
    });

    for (std::size_t i = 0; i < rating.size(); ++i) {
        rating[i].place = static_cast<std::int32_t>(i + 1);
    }

    return rating;
}

void ServerLogic::Impl::broadcast_rating_if_changed_locked(std::vector<RatingEntry>& pending_rating, const bool force) {
    auto rating = build_rating_locked();
    if (!force && rating == latest_rating) {
        return;
    }
    latest_rating = rating;
    pending_rating = std::move(rating);
}

void ServerLogic::Impl::notify_rating_subscribers(std::vector<RatingEntry> rating) {
    std::vector<RatingCallback> subscribers;
    {
        std::lock_guard lock{mutex};
        subscribers.reserve(rating_subscribers.size());
        for (const auto& [subscription_id, callback] : rating_subscribers) {
            subscribers.push_back(callback);
        }
    }
    for (auto& callback : subscribers) {
        callback(rating);
    }
}

}  // namespace server_logic
