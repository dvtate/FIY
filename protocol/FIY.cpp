#include <clocale>

#include <thread>

#include <unistd.h>

#include "Server/Server.hpp"

#include "FIY.hpp"

/**
 * Start the protocol server + portal
 * @param argc from main
 * @param argv from main
 * @return false on error
 */
bool FIY::start(const int argc, char* argv[]) {
    // Load config
    if (!config.from_argv(argc, argv)) {
        if (config.error) {
            LOG_ERR("Failed to load configuration.");
            return false;
        }
        return true; // --help
    }

    return start();
}

/**
 * Start the protocol server + portal
 * @param argc from main
 * @param argv from main
 * @return false on error
 */
bool FIY::start() {
    // Note: order is important

    // Make sure config loaded
    if (config.error) {
        LOG_ERR("Failed to parse config file.");
        return false;
    }

    // Track current time
    std::thread now_tracker{[this]() {
        // TODO this seems inefficient
        while (true) {
            m_now = std::time(nullptr);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }};

    // Start apps
    mods.find_modules();
    if (!mods.start_all()) {
        LOG_ERR("Failed to start apps.");
        // return false;
    }

    // Make Boost IO context
    m_ioc = new boost::asio::io_context{config.concurrency};

    // Initialize the server (it uses boost io context)
    Server::start(m_ioc);

    // Run the I/O service on the requested number of threads
    std::vector<std::thread> v;
    v.reserve(config.concurrency - 1);
    DEBUG_LOG("Using " << config.concurrency << " threads");
    for (auto i = config.concurrency - 1; i > 0; --i)
        v.emplace_back([this]{ m_ioc->run(); });
    m_ioc->run();

    return true;
}

// /**
//  * Restart the protocol server
//  */
// void FIY::restart() {
//     char buff[PATH_MAX];
//     if (realpath("/proc/self/exe", buff) != buff) {
//         perror("realpath");
//         LOG_ERR("Failed to get /proc/self/exe.");
//         strcpy(buff, "/proc/self/exe");
//     }
//     if ( execv( buff, nullptr) == -1 )
//         perror("execv");
//     LOG_ERR("Failed to restart");
// }

const std::string& FIY::base_uri() const {
    static const std::string ret = std::string(config.protocol)
        + config.hostname;
    return ret;
}
