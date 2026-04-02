#include "api_server/json_protocol.hpp"

#include "server_logic/server_logic.hpp"
#include "test_helpers.hpp"

#include <boost/json.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <string>

namespace json = boost::json;

namespace {

json::object parse_object_response(const std::string& response) {
    return json::parse(response).as_object();
}

void expect_error_code(const std::string& response, const std::string& expected_code) {
    const auto object = parse_object_response(response);
    EXPECT_EQ(object.at("type").as_string(), "error");
    EXPECT_EQ(object.at("code").as_string(), expected_code);
    EXPECT_TRUE(object.contains("message"));
}

}  // namespace

TEST(JsonProtocolTest, RejectsBrokenJson) {
    server_logic::ServerLogic logic{server_logic::ServerConfig{}};
    api_server::JsonProtocol protocol{logic};

    expect_error_code(protocol.handle_request_line("{"), "INVALID_JSON");
}

TEST(JsonProtocolTest, RejectsNonObjectRequest) {
    server_logic::ServerLogic logic{server_logic::ServerConfig{}};
    api_server::JsonProtocol protocol{logic};

    expect_error_code(protocol.handle_request_line(R"(["register"])"), "INVALID_REQUEST");
}

TEST(JsonProtocolTest, RejectsMissingTypeField) {
    server_logic::ServerLogic logic{server_logic::ServerConfig{}};
    api_server::JsonProtocol protocol{logic};

    expect_error_code(protocol.handle_request_line(R"({"login":"alex"})"), "INVALID_REQUEST");
}

TEST(JsonProtocolTest, RejectsUnknownRequestType) {
    server_logic::ServerLogic logic{server_logic::ServerConfig{}};
    api_server::JsonProtocol protocol{logic};

    expect_error_code(protocol.handle_request_line(R"({"type":"ping"})"), "INVALID_REQUEST");
}

TEST(JsonProtocolTest, RegistersUserAndReturnsExpectedPayload) {
    server_logic::ServerLogic logic{server_logic::ServerConfig{}};
    api_server::JsonProtocol protocol{logic};

    const auto object = parse_object_response(
        protocol.handle_request_line(R"({"type":"register","login":"alex","password":"qwerty123"})"));

    EXPECT_EQ(object.at("type").as_string(), "register_ok");
    EXPECT_EQ(object.at("playerId").as_int64(), 1);
    EXPECT_EQ(object.at("login").as_string(), "alex");
    EXPECT_EQ(object.at("coins").as_int64(), 20);
    EXPECT_EQ(object.at("machines").as_int64(), 1);
}

TEST(JsonProtocolTest, RejectsRegisterWithInvalidLogin) {
    server_logic::ServerLogic logic{server_logic::ServerConfig{}};
    api_server::JsonProtocol protocol{logic};

    expect_error_code(
        protocol.handle_request_line(R"({"type":"register","login":" ","password":"qwerty123"})"),
        "INVALID_LOGIN");
}

TEST(JsonProtocolTest, RejectsRegisterWithInvalidPassword) {
    server_logic::ServerLogic logic{server_logic::ServerConfig{}};
    api_server::JsonProtocol protocol{logic};

    expect_error_code(
        protocol.handle_request_line(R"({"type":"register","login":"alex","password":"short"})"),
        "INVALID_PASSWORD");
}

TEST(JsonProtocolTest, RejectsDuplicateRegister) {
    server_logic::ServerLogic logic{server_logic::ServerConfig{}};
    api_server::JsonProtocol protocol{logic};

    static_cast<void>(protocol.handle_request_line(R"({"type":"register","login":"alex","password":"qwerty123"})"));

    expect_error_code(
        protocol.handle_request_line(R"({"type":"register","login":"alex","password":"qwerty123"})"),
        "LOGIN_ALREADY_EXISTS");
}

TEST(JsonProtocolTest, RejectsRegisterWithMissingField) {
    server_logic::ServerLogic logic{server_logic::ServerConfig{}};
    api_server::JsonProtocol protocol{logic};

    expect_error_code(protocol.handle_request_line(R"({"type":"register","login":"alex"})"), "INVALID_REQUEST");
}

TEST(JsonProtocolTest, LogsInExistingUserAndReturnsExpectedPayload) {
    server_logic::ServerLogic logic{server_logic::ServerConfig{}};
    api_server::JsonProtocol protocol{logic};
    static_cast<void>(logic.register_user("alex", "qwerty123"));

    const auto object =
        parse_object_response(protocol.handle_request_line(R"({"type":"login","login":"alex","password":"qwerty123"})"));

    EXPECT_EQ(object.at("type").as_string(), "login_ok");
    EXPECT_EQ(object.at("playerId").as_int64(), 1);
    EXPECT_EQ(object.at("login").as_string(), "alex");
    EXPECT_EQ(object.at("coins").as_int64(), 20);
}

TEST(JsonProtocolTest, RejectsLoginForUnknownUser) {
    server_logic::ServerLogic logic{server_logic::ServerConfig{}};
    api_server::JsonProtocol protocol{logic};

    expect_error_code(
        protocol.handle_request_line(R"({"type":"login","login":"alex","password":"qwerty123"})"),
        "UNKNOWN_LOGIN");
}

TEST(JsonProtocolTest, RejectsLoginWithWrongPassword) {
    server_logic::ServerLogic logic{server_logic::ServerConfig{}};
    api_server::JsonProtocol protocol{logic};
    static_cast<void>(logic.register_user("alex", "qwerty123"));

    expect_error_code(
        protocol.handle_request_line(R"({"type":"login","login":"alex","password":"wrongpass"})"),
        "AUTH_FAILED");
}

TEST(JsonProtocolTest, RejectsLoginWithWrongFieldType) {
    server_logic::ServerLogic logic{server_logic::ServerConfig{}};
    api_server::JsonProtocol protocol{logic};

    expect_error_code(protocol.handle_request_line(R"({"type":"login","login":1,"password":"qwerty123"})"), "INVALID_REQUEST");
}

TEST(JsonProtocolTest, CreatesOrderAndReturnsOrderId) {
    server_logic::ServerLogic logic{server_logic::ServerConfig{.worker_pool_size = 0}};
    api_server::JsonProtocol protocol{logic};
    const auto auth = logic.register_user("alex", "qwerty123");

    const auto object = parse_object_response(protocol.handle_request_line(
        "{\"type\":\"create_order\",\"playerId\":" + std::to_string(auth.player_id) + ",\"item\":\"Gear\"}"));

    EXPECT_EQ(object.at("type").as_string(), "order_created");
    EXPECT_EQ(object.at("orderId").as_int64(), 1);
}

TEST(JsonProtocolTest, RejectsCreateOrderForUnknownPlayer) {
    server_logic::ServerLogic logic{server_logic::ServerConfig{.worker_pool_size = 0}};
    api_server::JsonProtocol protocol{logic};

    expect_error_code(protocol.handle_request_line(R"({"type":"create_order","playerId":42,"item":"Gear"})"), "UNKNOWN_PLAYER");
}

TEST(JsonProtocolTest, RejectsCreateOrderForInvalidItem) {
    server_logic::ServerLogic logic{server_logic::ServerConfig{.worker_pool_size = 0}};
    api_server::JsonProtocol protocol{logic};
    const auto auth = logic.register_user("alex", "qwerty123");

    expect_error_code(
        protocol.handle_request_line(
            "{\"type\":\"create_order\",\"playerId\":" + std::to_string(auth.player_id) + ",\"item\":\"Bad\"}"),
        "INVALID_ITEM");
}

TEST(JsonProtocolTest, RejectsCreateOrderWhenQueueIsFull) {
    server_logic::ServerConfig config;
    config.worker_pool_size = 0;
    config.max_queue_size = 1;
    server_logic::ServerLogic logic{config};
    api_server::JsonProtocol protocol{logic};
    const auto auth = logic.register_user("alex", "qwerty123");

    static_cast<void>(protocol.handle_request_line(
        "{\"type\":\"create_order\",\"playerId\":" + std::to_string(auth.player_id) + ",\"item\":\"Gear\"}"));

    expect_error_code(
        protocol.handle_request_line(
            "{\"type\":\"create_order\",\"playerId\":" + std::to_string(auth.player_id) + ",\"item\":\"Plate\"}"),
        "QUEUE_FULL");
}

TEST(JsonProtocolTest, ListsOrdersWithSerializedFields) {
    server_logic::ServerLogic logic{server_logic::ServerConfig{.worker_pool_size = 0}};
    api_server::JsonProtocol protocol{logic};
    const auto auth = logic.register_user("alex", "qwerty123");
    static_cast<void>(logic.create_order(auth.player_id, server_logic::ItemType::Gear));

    const auto object = parse_object_response(
        protocol.handle_request_line("{\"type\":\"list_orders\",\"playerId\":" + std::to_string(auth.player_id) + "}"));
    const auto& items = object.at("items").as_array();

    EXPECT_EQ(object.at("type").as_string(), "orders");
    ASSERT_EQ(items.size(), 1U);
    const auto& order = items[0].as_object();
    EXPECT_EQ(order.at("orderId").as_int64(), 1);
    EXPECT_EQ(order.at("item").as_string(), "Gear");
    EXPECT_EQ(order.at("status").as_string(), "QUEUED");
    EXPECT_EQ(order.at("progress").as_int64(), 0);
    EXPECT_EQ(order.at("rewardCoinsTotal").as_int64(), 2);
    EXPECT_TRUE(order.contains("createdAt"));
}

TEST(JsonProtocolTest, RejectsListOrdersForUnknownPlayer) {
    server_logic::ServerLogic logic{server_logic::ServerConfig{}};
    api_server::JsonProtocol protocol{logic};

    expect_error_code(protocol.handle_request_line(R"({"type":"list_orders","playerId":42})"), "UNKNOWN_PLAYER");
}

TEST(JsonProtocolTest, ReturnsStatsSnapshot) {
    server_logic::ServerLogic logic{server_logic::ServerConfig{.worker_pool_size = 0}};
    api_server::JsonProtocol protocol{logic};
    const auto auth = logic.register_user("alex", "qwerty123");
    static_cast<void>(logic.create_order(auth.player_id, server_logic::ItemType::Gear));

    const auto object =
        parse_object_response(protocol.handle_request_line("{\"type\":\"stats\",\"playerId\":" + std::to_string(auth.player_id) + "}"));

    EXPECT_EQ(object.at("type").as_string(), "stats");
    EXPECT_EQ(object.at("coins").as_int64(), 20);
    EXPECT_EQ(object.at("machines").as_int64(), 1);
    EXPECT_EQ(object.at("queued").as_int64(), 1);
    EXPECT_EQ(object.at("running").as_int64(), 0);
    EXPECT_EQ(object.at("done").as_int64(), 0);
    EXPECT_EQ(object.at("failed").as_int64(), 0);
}

TEST(JsonProtocolTest, RejectsStatsForUnknownPlayer) {
    server_logic::ServerLogic logic{server_logic::ServerConfig{}};
    api_server::JsonProtocol protocol{logic};

    expect_error_code(protocol.handle_request_line(R"({"type":"stats","playerId":42})"), "UNKNOWN_PLAYER");
}

TEST(JsonProtocolTest, BuysMachineAndReturnsUpdatedState) {
    server_logic::ServerConfig config;
    config.start_coins = 40;
    server_logic::ServerLogic logic{config};
    api_server::JsonProtocol protocol{logic};
    const auto auth = logic.register_user("alex", "qwerty123");

    const auto object = parse_object_response(
        protocol.handle_request_line("{\"type\":\"buy_machine\",\"playerId\":" + std::to_string(auth.player_id) + "}"));

    EXPECT_EQ(object.at("type").as_string(), "machine_bought");
    EXPECT_EQ(object.at("machines").as_int64(), 2);
    EXPECT_EQ(object.at("coins").as_int64(), 25);
}

TEST(JsonProtocolTest, RejectsBuyMachineForUnknownPlayer) {
    server_logic::ServerLogic logic{server_logic::ServerConfig{}};
    api_server::JsonProtocol protocol{logic};

    expect_error_code(protocol.handle_request_line(R"({"type":"buy_machine","playerId":42})"), "UNKNOWN_PLAYER");
}

TEST(JsonProtocolTest, RejectsBuyMachineWhenCoinsAreInsufficient) {
    server_logic::ServerConfig config;
    config.start_coins = 10;
    server_logic::ServerLogic logic{config};
    api_server::JsonProtocol protocol{logic};
    const auto auth = logic.register_user("alex", "qwerty123");

    expect_error_code(
        protocol.handle_request_line("{\"type\":\"buy_machine\",\"playerId\":" + std::to_string(auth.player_id) + "}"),
        "NOT_ENOUGH_COINS");
}

TEST(JsonProtocolTest, CancelsQueuedOrderAndReturnsConfirmation) {
    server_logic::ServerConfig config = test_helpers::make_async_config();
    config.worker_pool_size = 0;
    server_logic::ServerLogic logic{config};
    api_server::JsonProtocol protocol{logic};
    const auto auth = logic.register_user("alex", "qwerty123");
    const auto order_id = logic.create_order(auth.player_id, server_logic::ItemType::Gear);

    const auto object = parse_object_response(
        protocol.handle_request_line("{\"type\":\"cancel_order\",\"playerId\":" + std::to_string(auth.player_id) +
                                     ",\"orderId\":" + std::to_string(order_id) + "}"));

    EXPECT_EQ(object.at("type").as_string(), "order_cancelled");
    EXPECT_EQ(object.at("orderId").as_int64(), order_id);
}

TEST(JsonProtocolTest, RejectsCancelForUnknownOrder) {
    server_logic::ServerLogic logic{server_logic::ServerConfig{}};
    api_server::JsonProtocol protocol{logic};
    const auto auth = logic.register_user("alex", "qwerty123");

    expect_error_code(
        protocol.handle_request_line("{\"type\":\"cancel_order\",\"playerId\":" + std::to_string(auth.player_id) +
                                     ",\"orderId\":42}"),
        "UNKNOWN_ORDER");
}

TEST(JsonProtocolTest, RejectsCancelForForeignOrder) {
    server_logic::ServerConfig config;
    config.worker_pool_size = 0;
    server_logic::ServerLogic logic{config};
    api_server::JsonProtocol protocol{logic};
    const auto first = logic.register_user("alex", "qwerty123");
    const auto second = logic.register_user("max", "qwerty123");
    const auto order_id = logic.create_order(second.player_id, server_logic::ItemType::Gear);

    expect_error_code(
        protocol.handle_request_line("{\"type\":\"cancel_order\",\"playerId\":" + std::to_string(first.player_id) +
                                     ",\"orderId\":" + std::to_string(order_id) + "}"),
        "FORBIDDEN");
}

TEST(JsonProtocolTest, RejectsCancelForCompletedOrder) {
    server_logic::ServerLogic logic{test_helpers::make_async_config()};
    api_server::JsonProtocol protocol{logic};
    const auto auth = logic.register_user("alex", "qwerty123");
    const auto order_id = logic.create_order(auth.player_id, server_logic::ItemType::Gear);
    ASSERT_TRUE(logic.wait_for_order_terminal_state(order_id, std::chrono::milliseconds{1000}));

    expect_error_code(
        protocol.handle_request_line("{\"type\":\"cancel_order\",\"playerId\":" + std::to_string(auth.player_id) +
                                     ",\"orderId\":" + std::to_string(order_id) + "}"),
        "CANNOT_CANCEL");
}

TEST(JsonProtocolTest, ReturnsRatingPayload) {
    server_logic::ServerLogic logic{test_helpers::make_async_config()};
    api_server::JsonProtocol protocol{logic};
    static_cast<void>(logic.register_user("alex", "qwerty123"));
    const auto second = logic.register_user("max", "qwerty123");

    const auto order_id = logic.create_order(second.player_id, server_logic::ItemType::Circuit);
    ASSERT_TRUE(logic.wait_for_order_terminal_state(order_id, std::chrono::milliseconds{1000}));

    const auto object = parse_object_response(
        protocol.handle_request_line("{\"type\":\"rating\",\"playerId\":" + std::to_string(second.player_id) + "}"));
    const auto& items = object.at("items").as_array();

    EXPECT_EQ(object.at("type").as_string(), "rating");
    ASSERT_EQ(items.size(), 2U);
    EXPECT_EQ(items[0].as_object().at("playerId").as_int64(), second.player_id);
    EXPECT_EQ(items[0].as_object().at("doneOrders").as_int64(), 1);
}

TEST(JsonProtocolTest, RejectsRatingForUnknownPlayer) {
    server_logic::ServerLogic logic{server_logic::ServerConfig{}};
    api_server::JsonProtocol protocol{logic};

    expect_error_code(protocol.handle_request_line(R"({"type":"rating","playerId":42})"), "UNKNOWN_PLAYER");
}

TEST(JsonProtocolTest, BuildsRatingUpdatedPushMessage) {
    server_logic::ServerLogic logic{server_logic::ServerConfig{}};
    api_server::JsonProtocol protocol{logic};

    const auto response = protocol.make_rating_updated_message({
        server_logic::RatingEntry{
            .place = 1,
            .player_id = 7,
            .login = "alex",
            .coins = 25,
            .done_orders = 2,
            .rating_score = 45,
        },
    });

    const auto object = parse_object_response(response);
    const auto& items = object.at("items").as_array();

    EXPECT_EQ(object.at("type").as_string(), "rating_updated");
    ASSERT_EQ(items.size(), 1U);
    EXPECT_EQ(items[0].as_object().at("playerId").as_int64(), 7);
    EXPECT_EQ(items[0].as_object().at("login").as_string(), "alex");
}
