#include "api_server/client_session.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>

#include <istream>
#include <utility>

namespace api_server {

using boost::asio::ip::tcp;

ClientSession::ClientSession(tcp::socket socket, MessageHandler on_message, CloseHandler on_close)
    : socket_(std::move(socket)),
      strand_(boost::asio::make_strand(socket_.get_executor())),
      on_message_(std::move(on_message)),
      on_close_(std::move(on_close)) {}

void ClientSession::start() {
    boost::asio::dispatch(strand_, [self = shared_from_this()] {
        self->do_read();
    });
}

void ClientSession::deliver(std::string message) {
    boost::asio::post(strand_, [self = shared_from_this(), message = std::move(message)]() mutable {
        if (self->closed_) {
            return;
        }

        const bool write_in_progress = !self->write_queue_.empty();
        self->write_queue_.push_back(std::move(message));
        if (!write_in_progress) {
            self->do_write();
        }
    });
}

void ClientSession::stop() {
    boost::asio::post(strand_, [self = shared_from_this()] {
        self->close();
    });
}

void ClientSession::do_read() {
    boost::asio::async_read_until(
        socket_,
        read_buffer_,
        '\n',
        boost::asio::bind_executor(
            strand_,
            [self = shared_from_this()](const boost::system::error_code& error, std::size_t /*bytes_transferred*/) {
                if (error) {
                    self->close();
                    return;
                }

                std::istream input{&self->read_buffer_};
                std::string line;
                std::getline(input, line);

                if (self->on_message_) {
                    self->on_message_(self, line);
                }

                self->do_read();
            }));
}

void ClientSession::do_write() {
    boost::asio::async_write(
        socket_,
        boost::asio::buffer(write_queue_.front()),
        boost::asio::bind_executor(
            strand_,
            [self = shared_from_this()](const boost::system::error_code& error, std::size_t /*bytes_transferred*/) {
                if (error) {
                    self->close();
                    return;
                }

                self->write_queue_.pop_front();
                if (!self->write_queue_.empty()) {
                    self->do_write();
                }
            }));
}

void ClientSession::close() {
    if (closed_) {
        return;
    }
    closed_ = true;

    boost::system::error_code ignored_error;
    socket_.shutdown(tcp::socket::shutdown_both, ignored_error);
    socket_.close(ignored_error);

    if (on_close_) {
        on_close_(shared_from_this());
    }
}

}  // namespace api_server
