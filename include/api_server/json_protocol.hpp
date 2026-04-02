#pragma once

#include <boost/json/value.hpp>

#include "server_logic/server_logic.hpp"

#include <string>

namespace api_server {

class JsonProtocol {
public:
    explicit JsonProtocol(server_logic::ServerLogic& logic);

    [[nodiscard]] std::string handle_request_line(const std::string& line) const;
    [[nodiscard]] std::string make_rating_updated_message(
        const std::vector<server_logic::RatingEntry>& rating) const;

private:
    [[nodiscard]] std::string handle_request_json(const boost::json::value& request) const;
    [[nodiscard]] std::string handle_register(const boost::json::object& request) const;
    [[nodiscard]] std::string handle_login(const boost::json::object& request) const;
    [[nodiscard]] std::string handle_create_order(const boost::json::object& request) const;
    [[nodiscard]] std::string handle_list_orders(const boost::json::object& request) const;
    [[nodiscard]] std::string handle_stats(const boost::json::object& request) const;
    [[nodiscard]] std::string handle_buy_machine(const boost::json::object& request) const;
    [[nodiscard]] std::string handle_cancel_order(const boost::json::object& request) const;
    [[nodiscard]] std::string handle_rating(const boost::json::object& request) const;

    server_logic::ServerLogic& logic_;
};

}  // namespace api_server
