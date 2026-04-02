#include "api_server/tcp_server.hpp"

#include <boost/asio/post.hpp>

#include <utility>
#include <vector>

namespace api_server {

using boost::asio::ip::tcp;

TcpServer::TcpServer(boost::asio::io_context& io_context, server_logic::ServerLogic& logic, unsigned short port)
    : io_context_(io_context),
      acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
      logic_(logic),
      protocol_(logic) {}

unsigned short TcpServer::port() const {
    return acceptor_.local_endpoint().port();
}

void TcpServer::start() {
    if (started_) {
        return;
    }
    started_ = true;

    acceptor_.set_option(tcp::acceptor::reuse_address(true));
    rating_subscription_id_ = logic_.subscribe_rating_updates(
        [this](std::vector<server_logic::RatingEntry> rating) {
            broadcast_rating_update(rating);
        });
    do_accept();
}

void TcpServer::stop() {
    if (!started_) {
        return;
    }
    started_ = false;

    boost::system::error_code ignored_error;
    acceptor_.close(ignored_error);

    if (rating_subscription_id_ != 0) {
        logic_.unsubscribe_rating_updates(rating_subscription_id_);
        rating_subscription_id_ = 0;
    }

    std::vector<std::shared_ptr<ClientSession>> sessions_to_stop;
    {
        std::lock_guard lock{sessions_mutex_};
        sessions_to_stop.assign(sessions_.begin(), sessions_.end());
        sessions_.clear();
    }

    for (const auto& session : sessions_to_stop) {
        if (session) {
            session->stop();
        }
    }
}

void TcpServer::do_accept() {
    acceptor_.async_accept([this](const boost::system::error_code& error, tcp::socket socket) {
        if (!acceptor_.is_open()) {
            return;
        }

        if (!error) {
            auto session = std::make_shared<ClientSession>(
                std::move(socket),
                [this](const std::shared_ptr<ClientSession>& current_session, const std::string& line) {
                    on_session_message(current_session, line);
                },
                [this](const std::shared_ptr<ClientSession>& current_session) {
                    on_session_closed(current_session);
                });

            {
                std::lock_guard lock{sessions_mutex_};
                sessions_.insert(session);
            }

            session->start();
        }

        do_accept();
    });
}

void TcpServer::on_session_message(const std::shared_ptr<ClientSession>& session, const std::string& line) {
    if (!session) {
        return;
    }
    session->deliver(protocol_.handle_request_line(line));
}

void TcpServer::on_session_closed(const std::shared_ptr<ClientSession>& session) {
    if (!session) {
        return;
    }
    std::lock_guard lock{sessions_mutex_};
    sessions_.erase(session);
}

void TcpServer::broadcast_rating_update(const std::vector<server_logic::RatingEntry>& rating) {
    const auto message = protocol_.make_rating_updated_message(rating);
    std::vector<std::shared_ptr<ClientSession>> sessions_copy;
    {
        std::lock_guard lock{sessions_mutex_};
        sessions_copy.assign(sessions_.begin(), sessions_.end());
    }

    for (const auto& session : sessions_copy) {
        if (session) {
            session->deliver(message);
        }
    }
}

}  // namespace api_server
