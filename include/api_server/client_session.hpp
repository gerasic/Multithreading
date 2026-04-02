#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/streambuf.hpp>

#include <deque>
#include <functional>
#include <memory>
#include <string>

namespace api_server {

class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    using MessageHandler = std::function<void(const std::shared_ptr<ClientSession>&, const std::string&)>;
    using CloseHandler = std::function<void(const std::shared_ptr<ClientSession>&)>;

    ClientSession(boost::asio::ip::tcp::socket socket, MessageHandler on_message, CloseHandler on_close);

    void start();
    void deliver(std::string message);
    void stop();

private:
    void do_read();
    void do_write();
    void close();

    boost::asio::ip::tcp::socket socket_;
    boost::asio::strand<boost::asio::any_io_executor> strand_; // гарантия обработчики сессии не будут одновременно лезть в её состояние
    boost::asio::streambuf read_buffer_;
    std::deque<std::string> write_queue_;
    MessageHandler on_message_;
    CloseHandler on_close_;
    bool closed_{false};
};

}  // namespace api_server
