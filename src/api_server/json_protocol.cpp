#include "api_server/json_protocol.hpp"

#include <boost/json.hpp>

#include "server_logic/error.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace api_server {

namespace {

namespace json = boost::json;

std::string serialize_with_newline(const json::value& value) {
    return json::serialize(value) + "\n";
}

std::string make_error_response(std::string_view code, std::string_view message) {
    return serialize_with_newline(json::object{
        {"type", "error"},
        {"code", code},
        {"message", message},
    });
}

std::string error_code_to_string(const server_logic::ErrorCode code) {
    using server_logic::ErrorCode;

    switch (code) {
    case ErrorCode::InvalidLogin:
        return "INVALID_LOGIN";
    case ErrorCode::InvalidPassword:
        return "INVALID_PASSWORD";
    case ErrorCode::LoginAlreadyExists:
        return "LOGIN_ALREADY_EXISTS";
    case ErrorCode::UnknownLogin:
        return "UNKNOWN_LOGIN";
    case ErrorCode::AuthFailed:
        return "AUTH_FAILED";
    case ErrorCode::UnknownPlayer:
        return "UNKNOWN_PLAYER";
    case ErrorCode::InvalidItem:
        return "INVALID_ITEM";
    case ErrorCode::QueueFull:
        return "QUEUE_FULL";
    case ErrorCode::NotEnoughCoins:
        return "NOT_ENOUGH_COINS";
    case ErrorCode::UnknownOrder:
        return "UNKNOWN_ORDER";
    case ErrorCode::Forbidden:
        return "FORBIDDEN";
    case ErrorCode::CannotCancel:
        return "CANNOT_CANCEL";
    }

    return "INTERNAL_ERROR";
}

const json::object& require_object(const json::value& value) {
    if (!value.is_object()) {
        throw std::invalid_argument{"Request must be a JSON object"};
    }
    return value.as_object();
}

std::string require_string_field(const json::object& object, std::string_view field) {
    const auto it = object.find(field);
    if (it == object.end() || !it->value().is_string()) {
        throw std::invalid_argument{"Field '" + std::string(field) + "' must be a string"};
    }
    return std::string(it->value().as_string());
}

std::int64_t require_int64_field(const json::object& object, std::string_view field) {
    const auto it = object.find(field);
    if (it == object.end() || (!it->value().is_int64() && !it->value().is_uint64())) {
        throw std::invalid_argument{"Field '" + std::string(field) + "' must be an integer"};
    }
    if (it->value().is_int64()) {
        return it->value().as_int64();
    }

    const auto value = it->value().as_uint64();
    if (value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        throw std::invalid_argument{"Field '" + std::string(field) + "' is out of range"};
    }
    return static_cast<std::int64_t>(value);
}

std::string item_type_to_string(const server_logic::ItemType item) {
    using server_logic::ItemType;

    switch (item) {
    case ItemType::Gear:
        return "Gear";
    case ItemType::Plate:
        return "Plate";
    case ItemType::Circuit:
        return "Circuit";
    }

    throw std::invalid_argument{"Unsupported item"};
}

server_logic::ItemType item_type_from_string(std::string_view item) {
    if (item == "Gear") {
        return server_logic::ItemType::Gear;
    }
    if (item == "Plate") {
        return server_logic::ItemType::Plate;
    }
    if (item == "Circuit") {
        return server_logic::ItemType::Circuit;
    }
    throw server_logic::ServerError{server_logic::ErrorCode::InvalidItem, "Unknown item"};
}

std::string order_status_to_string(const server_logic::OrderStatus status) {
    using server_logic::OrderStatus;

    switch (status) {
    case OrderStatus::Queued:
        return "QUEUED";
    case OrderStatus::Running:
        return "RUNNING";
    case OrderStatus::Done:
        return "DONE";
    case OrderStatus::Failed:
        return "FAILED";
    case OrderStatus::Cancelled:
        return "CANCELLED";
    }

    return "FAILED";
}

json::object make_auth_response(std::string_view type, const server_logic::AuthResult& result) {
    return json::object{
        {"type", type},
        {"playerId", result.player_id},
        {"login", result.login},
        {"coins", result.coins},
        {"machines", result.machines},
    };
}

json::object make_order_json(const server_logic::OrderInfo& order) {
    json::object result{
        {"orderId", order.order_id},
        {"item", item_type_to_string(order.item)},
        {"status", order_status_to_string(order.status)},
        {"progress", order.progress},
        {"rewardCoinsTotal", order.reward_coins_total},
        {"createdAt", order.created_at},
    };

    if (order.started_at.has_value()) {
        result["startedAt"] = *order.started_at;
    }
    if (order.finished_at.has_value()) {
        result["finishedAt"] = *order.finished_at;
    }

    return result;
}

json::object make_stats_json(const server_logic::StatsSnapshot& stats) {
    return json::object{
        {"type", "stats"},
        {"coins", stats.coins},
        {"machines", stats.machines},
        {"queued", stats.queued},
        {"running", stats.running},
        {"done", stats.done},
        {"failed", stats.failed},
    };
}

json::object make_buy_machine_json(const server_logic::BuyMachineResult& result) {
    return json::object{
        {"type", "machine_bought"},
        {"machines", result.machines},
        {"coins", result.coins},
    };
}

json::object make_rating_entry_json(const server_logic::RatingEntry& entry) {
    return json::object{
        {"place", entry.place},
        {"playerId", entry.player_id},
        {"login", entry.login},
        {"coins", entry.coins},
        {"doneOrders", entry.done_orders},
        {"ratingScore", entry.rating_score},
    };
}

json::array make_rating_array(const std::vector<server_logic::RatingEntry>& rating) {
    json::array items;
    items.reserve(rating.size());
    for (const auto& entry : rating) {
        items.emplace_back(make_rating_entry_json(entry));
    }
    return items;
}

}  // namespace

JsonProtocol::JsonProtocol(server_logic::ServerLogic& logic) : logic_(logic) {}

std::string JsonProtocol::handle_request_line(const std::string& line) const {
    try {
        const auto request = json::parse(line);
        return handle_request_json(request);
    } catch (const boost::system::system_error& error) {
        return make_error_response("INVALID_JSON", error.what());
    } catch (const server_logic::ServerError& error) {
        return make_error_response(error_code_to_string(error.code()), error.what());
    } catch (const std::exception& error) {
        return make_error_response("INVALID_REQUEST", error.what());
    }
}

std::string JsonProtocol::make_rating_updated_message(
    const std::vector<server_logic::RatingEntry>& rating) const {
    return serialize_with_newline(json::object{
        {"type", "rating_updated"},
        {"items", make_rating_array(rating)},
    });
}

std::string JsonProtocol::handle_request_json(const json::value& request) const {
    const auto& object = require_object(request);
    const auto type = require_string_field(object, "type");

    if (type == "register") {
        return handle_register(object);
    }
    if (type == "login") {
        return handle_login(object);
    }
    if (type == "create_order") {
        return handle_create_order(object);
    }
    if (type == "list_orders") {
        return handle_list_orders(object);
    }
    if (type == "stats") {
        return handle_stats(object);
    }
    if (type == "buy_machine") {
        return handle_buy_machine(object);
    }
    if (type == "cancel_order") {
        return handle_cancel_order(object);
    }
    if (type == "rating") {
        return handle_rating(object);
    }

    throw std::invalid_argument{"Unknown request type"};
}

std::string JsonProtocol::handle_register(const json::object& request) const {
    const auto login = require_string_field(request, "login");
    const auto password = require_string_field(request, "password");
    const auto result = logic_.register_user(login, password);
    return serialize_with_newline(make_auth_response("register_ok", result));
}

std::string JsonProtocol::handle_login(const json::object& request) const {
    const auto login = require_string_field(request, "login");
    const auto password = require_string_field(request, "password");
    const auto result = logic_.login(login, password);
    return serialize_with_newline(make_auth_response("login_ok", result));
}

std::string JsonProtocol::handle_create_order(const json::object& request) const {
    const auto player_id = require_int64_field(request, "playerId");
    const auto item = item_type_from_string(require_string_field(request, "item"));
    const auto order_id = logic_.create_order(player_id, item);

    return serialize_with_newline(json::object{
        {"type", "order_created"},
        {"orderId", order_id},
    });
}

std::string JsonProtocol::handle_list_orders(const json::object& request) const {
    const auto player_id = require_int64_field(request, "playerId");
    const auto orders = logic_.list_orders(player_id);

    json::array items;
    items.reserve(orders.size());
    for (const auto& order : orders) {
        items.emplace_back(make_order_json(order));
    }

    return serialize_with_newline(json::object{
        {"type", "orders"},
        {"items", std::move(items)},
    });
}

std::string JsonProtocol::handle_stats(const json::object& request) const {
    const auto player_id = require_int64_field(request, "playerId");
    return serialize_with_newline(make_stats_json(logic_.get_stats(player_id)));
}

std::string JsonProtocol::handle_buy_machine(const json::object& request) const {
    const auto player_id = require_int64_field(request, "playerId");
    return serialize_with_newline(make_buy_machine_json(logic_.buy_machine(player_id)));
}

std::string JsonProtocol::handle_cancel_order(const json::object& request) const {
    const auto player_id = require_int64_field(request, "playerId");
    const auto order_id = require_int64_field(request, "orderId");
    logic_.cancel_order(player_id, order_id);

    return serialize_with_newline(json::object{
        {"type", "order_cancelled"},
        {"orderId", order_id},
    });
}

std::string JsonProtocol::handle_rating(const json::object& request) const {
    const auto player_id = require_int64_field(request, "playerId");
    return serialize_with_newline(json::object{
        {"type", "rating"},
        {"items", make_rating_array(logic_.get_rating(player_id))},
    });
}

}  // namespace api_server
