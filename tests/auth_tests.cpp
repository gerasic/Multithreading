#include "test_helpers.hpp"

#include <gtest/gtest.h>

using server_logic::ErrorCode;
using server_logic::ServerConfig;
using server_logic::ServerError;
using server_logic::ServerLogic;

TEST(ServerLogicAuthTest, RegistersNewPlayerWithDefaultResources) {
    ServerLogic logic{ServerConfig{}};

    const auto result = logic.register_user("alex", "qwerty123");

    EXPECT_EQ(result.player_id, 1);
    EXPECT_EQ(result.login, "alex");
    EXPECT_EQ(result.coins, 20);
    EXPECT_EQ(result.machines, 1);
}

TEST(ServerLogicAuthTest, LogsIntoExistingPlayerWithoutCreatingNewOne) {
    ServerLogic logic{ServerConfig{}};

    const auto registered = logic.register_user("alex", "qwerty123");
    const auto logged = logic.login("alex", "qwerty123");

    EXPECT_EQ(logged.player_id, registered.player_id);
    EXPECT_EQ(logged.login, "alex");
    EXPECT_EQ(logged.coins, 20);
}

TEST(ServerLogicAuthTest, RejectsDuplicateLoginDuringRegistration) {
    ServerLogic logic{ServerConfig{}};
    static_cast<void>(logic.register_user("alex", "qwerty123"));

    try {
        static_cast<void>(logic.register_user("alex", "another123"));
        FAIL();
    } catch (const ServerError& error) {
        EXPECT_EQ(error.code(), ErrorCode::LoginAlreadyExists);
    }
}

TEST(ServerLogicAuthTest, RejectsInvalidLoginDuringRegistration) {
    ServerLogic logic{ServerConfig{}};

    try {
        static_cast<void>(logic.register_user(" ", "qwerty123"));
        FAIL();
    } catch (const ServerError& error) {
        EXPECT_EQ(error.code(), ErrorCode::InvalidLogin);
    }
}

TEST(ServerLogicAuthTest, RejectsShortPasswordDuringRegistration) {
    ServerLogic logic{ServerConfig{}};

    try {
        static_cast<void>(logic.register_user("alex", "short"));
        FAIL();
    } catch (const ServerError& error) {
        EXPECT_EQ(error.code(), ErrorCode::InvalidPassword);
    }
}

TEST(ServerLogicAuthTest, RejectsUnknownLoginDuringAuthentication) {
    ServerLogic logic{ServerConfig{}};

    try {
        static_cast<void>(logic.login("alex", "qwerty123"));
        FAIL();
    } catch (const ServerError& error) {
        EXPECT_EQ(error.code(), ErrorCode::UnknownLogin);
    }
}

TEST(ServerLogicAuthTest, RejectsWrongPasswordDuringAuthentication) {
    ServerLogic logic{ServerConfig{}};
    static_cast<void>(logic.register_user("alex", "qwerty123"));

    try {
        static_cast<void>(logic.login("alex", "wrongpass"));
        FAIL();
    } catch (const ServerError& error) {
        EXPECT_EQ(error.code(), ErrorCode::AuthFailed);
    }
}
