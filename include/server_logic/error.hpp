#pragma once

#include <stdexcept>
#include <string>

namespace server_logic {

enum class ErrorCode {
    InvalidLogin,
    InvalidPassword,
    LoginAlreadyExists,
    UnknownLogin,
    AuthFailed,
    UnknownPlayer,
    InvalidItem,
    QueueFull,
    NotEnoughCoins,
    UnknownOrder,
    Forbidden,
    CannotCancel
};

class ServerError final : public std::runtime_error {
public:
    ServerError(ErrorCode code, std::string message);

    [[nodiscard]] ErrorCode code() const noexcept;

private:
    ErrorCode code_;
};

}  // namespace server_logic
