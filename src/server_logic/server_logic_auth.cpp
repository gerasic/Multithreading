#include "server_logic_impl.hpp"

#include <utility>

namespace server_logic {

AuthResult ServerLogic::register_user(const std::string& login, const std::string& password) {
    if (!impl_->is_valid_login(login)) {
        throw ServerError{ErrorCode::InvalidLogin, "Login is invalid"};
    }
    if (!impl_->is_valid_password(password)) {
        throw ServerError{ErrorCode::InvalidPassword, "Password is invalid"};
    }

    std::vector<RatingEntry> pending_rating;
    AuthResult result;
    {
        std::lock_guard lock{impl_->mutex};
        if (impl_->player_ids_by_login.contains(login)) {
            throw ServerError{ErrorCode::LoginAlreadyExists, "Login already exists"};
        }

        const auto player_id = impl_->next_player_id++;
        auto [it, inserted] = impl_->players.emplace(player_id, Impl::PlayerState{
            .player =
                Player{
                    .player_id = player_id,
                    .login = login,
                    .coins = impl_->config.start_coins,
                    .machines = impl_->config.start_machines,
                },
            .password_hash = Impl::hash_password(login, password),
        });
        if (!inserted) {
            throw std::logic_error{"Failed to create player"};
        }
        impl_->player_ids_by_login.emplace(login, player_id);
        result = AuthResult{
            .player_id = player_id,
            .login = login,
            .coins = it->second.player.coins,
            .machines = it->second.player.machines,
        };
        impl_->broadcast_rating_if_changed_locked(pending_rating, true);
    }
    if (!pending_rating.empty()) {
        impl_->notify_rating_subscribers(std::move(pending_rating));
    }
    return result;
}

AuthResult ServerLogic::login(const std::string& login_name, const std::string& password) const {
    if (!impl_->is_valid_login(login_name)) {
        throw ServerError{ErrorCode::InvalidLogin, "Login is invalid"};
    }
    if (!impl_->is_valid_password(password)) {
        throw ServerError{ErrorCode::InvalidPassword, "Password is invalid"};
    }

    std::lock_guard lock{impl_->mutex};
    const auto login_it = impl_->player_ids_by_login.find(login_name);
    if (login_it == impl_->player_ids_by_login.end()) {
        throw ServerError{ErrorCode::UnknownLogin, "Login not found"};
    }

    const auto& player_state = impl_->require_player_state(login_it->second);
    if (player_state.password_hash != Impl::hash_password(login_name, password)) {
        throw ServerError{ErrorCode::AuthFailed, "Authentication failed"};
    }

    return AuthResult{
        .player_id = player_state.player.player_id,
        .login = player_state.player.login,
        .coins = player_state.player.coins,
        .machines = player_state.player.machines,
    };
}

}  // namespace server_logic
