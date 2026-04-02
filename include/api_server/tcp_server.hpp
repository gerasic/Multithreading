#pragma once

#include "api_server/client_session.hpp"
#include "api_server/json_protocol.hpp"
#include "server_logic/server_logic.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <memory>
#include <mutex>
#include <unordered_set>

namespace api_server {

class TcpServer {
public:
    TcpServer(boost::asio::io_context& io_context, server_logic::ServerLogic& logic, unsigned short port);

    [[nodiscard]] unsigned short port() const;
    void start();
    void stop();

private:
    void do_accept();
    void on_session_message(const std::shared_ptr<ClientSession>& session, const std::string& line);
    void on_session_closed(const std::shared_ptr<ClientSession>& session);
    void broadcast_rating_update(const std::vector<server_logic::RatingEntry>& rating);

    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    server_logic::ServerLogic& logic_;
    JsonProtocol protocol_;
    std::mutex sessions_mutex_;
    std::unordered_set<std::shared_ptr<ClientSession>> sessions_;
    std::size_t rating_subscription_id_{0};
    bool started_{false};
};

}  // namespace api_server
