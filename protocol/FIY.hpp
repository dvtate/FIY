#pragma once

#include <ctime>

#include "defs.hpp"

#include "peers/Peers.hpp"
#include "Config.hpp"
#include "mods/Mods.hpp"
#include "users/LocalUsers.hpp"
#include "HttpClient.hpp"
#include "HttpsClient.hpp"

// Global Singleton set in main.cpp
class FIY;
extern FIY* g_fiy;

/**
 * Global protocol server app singleton
 */
class FIY {
    std::time_t m_now{0};

    friend class HttpClient;
    friend class HttpsClient;
    boost::asio::io_context* m_ioc{nullptr};

public:
    FiyConfig config;
    Peers peers;
    Mods mods;
    LocalUsers users;
    HttpClient http;
    HttpsClient https;

    FIY() = default;
    explicit FIY(const std::string& config_path):
        config(config_path)
    {}

    bool start(int argc, char* argv[]);
    bool start();
    // static void restart();

    std::time_t now() const {
        return m_now;
    }

    [[nodiscard]] const std::string& base_uri() const;
};
