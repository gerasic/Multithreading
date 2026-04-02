#include "api_server/tcp_server.hpp"

#include "server_logic/server_logic.hpp"
#include "test_helpers.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>
#include <boost/json.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <istream>
#include <stdexcept>
#include <string>
#include <thread>

namespace json = boost::json;
using boost::asio::ip::tcp;

namespace {

std::string read_line(tcp::socket& socket, boost::asio::streambuf& buffer) {
    boost::asio::read_until(socket, buffer, '\n');
    std::istream input{&buffer};
    std::string line;
    std::getline(input, line);
    return line;
}

json::object send_request_and_read_response(
    tcp::socket& socket,
    boost::asio::streambuf& buffer,
    std::string request,
    std::string_view expected_type) {
    request.push_back('\n');
    boost::asio::write(socket, boost::asio::buffer(request));

    while (true) {
        auto object = json::parse(read_line(socket, buffer)).as_object();
        if (object.at("type").as_string() == expected_type) {
            return object;
        }
        if (object.at("type").as_string() != "rating_updated") {
            throw std::runtime_error("Received unexpected response type while waiting for request result");
        }
    }
}

class TcpServerFixture : public ::testing::Test {
protected:
    TcpServerFixture() : logic_(test_helpers::make_async_config()), server_(server_io_context_, logic_, 0) {}

    void SetUp() override {
        server_.start();
        server_thread_ = std::thread([this] {
            server_io_context_.run();
        });

        client_socket_ = std::make_unique<tcp::socket>(client_io_context_);
        client_socket_->connect(tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), server_.port()));
    }

    void TearDown() override {
        if (client_socket_ && client_socket_->is_open()) {
            boost::system::error_code ignored_error;
            client_socket_->shutdown(tcp::socket::shutdown_both, ignored_error);
            client_socket_->close(ignored_error);
        }

        server_.stop();
        server_io_context_.stop();
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }

    json::object send_request(const std::string& request, std::string_view expected_type) {
        return send_request_and_read_response(*client_socket_, client_read_buffer_, request, expected_type);
    }

    json::object read_message_of_type(std::string_view expected_type) {
        while (true) {
            auto object = json::parse(read_line(*client_socket_, client_read_buffer_)).as_object();
            if (object.at("type").as_string() == expected_type) {
                return object;
            }
        }
    }

    boost::asio::io_context server_io_context_;
    boost::asio::io_context client_io_context_;
    server_logic::ServerLogic logic_;
    api_server::TcpServer server_;
    std::thread server_thread_;
    std::unique_ptr<tcp::socket> client_socket_;
    boost::asio::streambuf client_read_buffer_;
};

}  // namespace

TEST_F(TcpServerFixture, HandlesRegisterRequestOverTcp) {
    const auto response = send_request(R"({"type":"register","login":"alex","password":"qwerty123"})", "register_ok");

    EXPECT_EQ(response.at("type").as_string(), "register_ok");
    EXPECT_EQ(response.at("playerId").as_int64(), 1);
    EXPECT_EQ(response.at("login").as_string(), "alex");
}

TEST_F(TcpServerFixture, ReturnsErrorForBrokenJsonOverTcp) {
    const auto response = send_request("{", "error");

    EXPECT_EQ(response.at("type").as_string(), "error");
    EXPECT_EQ(response.at("code").as_string(), "INVALID_JSON");
}

TEST_F(TcpServerFixture, HandlesOrderLifecycleQueriesOverTcp) {
    const auto auth = logic_.register_user("alex", "qwerty123");

    const auto created = send_request(
        "{\"type\":\"create_order\",\"playerId\":" + std::to_string(auth.player_id) + ",\"item\":\"Gear\"}",
        "order_created");
    EXPECT_EQ(created.at("type").as_string(), "order_created");
    EXPECT_EQ(created.at("orderId").as_int64(), 1);

    const auto orders =
        send_request("{\"type\":\"list_orders\",\"playerId\":" + std::to_string(auth.player_id) + "}", "orders");
    ASSERT_EQ(orders.at("items").as_array().size(), 1U);
    EXPECT_EQ(orders.at("items").as_array()[0].as_object().at("item").as_string(), "Gear");

    const auto stats = send_request("{\"type\":\"stats\",\"playerId\":" + std::to_string(auth.player_id) + "}", "stats");
    EXPECT_EQ(stats.at("type").as_string(), "stats");
    EXPECT_EQ(stats.at("queued").as_int64() + stats.at("running").as_int64() + stats.at("done").as_int64(), 1);
}

TEST_F(TcpServerFixture, ReturnsBusinessErrorOverTcp) {
    const auto auth = logic_.register_user("alex", "qwerty123");
    static_cast<void>(auth);

    const auto success = send_request(
        "{\"type\":\"buy_machine\",\"playerId\":" + std::to_string(auth.player_id) + "}",
        "machine_bought");
    EXPECT_EQ(success.at("type").as_string(), "machine_bought");

    const auto response = send_request(
        "{\"type\":\"buy_machine\",\"playerId\":" + std::to_string(auth.player_id) + "}",
        "error");

    EXPECT_EQ(response.at("type").as_string(), "error");
    EXPECT_EQ(response.at("code").as_string(), "NOT_ENOUGH_COINS");
}

TEST_F(TcpServerFixture, PushesRatingUpdatedToConnectedClient) {
    const auto alex = logic_.register_user("alex", "qwerty123");
    const auto max = logic_.register_user("max", "qwerty123");

    const auto rating =
        send_request("{\"type\":\"rating\",\"playerId\":" + std::to_string(alex.player_id) + "}", "rating");
    EXPECT_EQ(rating.at("type").as_string(), "rating");

    const auto order_id = logic_.create_order(max.player_id, server_logic::ItemType::Circuit);
    ASSERT_TRUE(logic_.wait_for_order_terminal_state(order_id, std::chrono::milliseconds{1000}));

    const auto push = read_message_of_type("rating_updated");
    EXPECT_EQ(push.at("type").as_string(), "rating_updated");
    ASSERT_FALSE(push.at("items").as_array().empty());
}
