//
// Created by tate on 4/10/26.
//

#pragma once

#include "ModConnector.hpp"

// IPC over the network
class ModConnectorNet : public ModConnector {
    std::string m_bearer_send;
    std::string m_bearer_accept;
    bool m_https;

public:
    // TODO uri could have a subpath component that needs to be separated
    ModConnectorNet(Mod* mod, std::string path, const bool is_https = false):
        ModConnector(mod, std::move(path)),
        m_https(is_https)
    {}

    bool start() override;
    bool stop() override;
    void delete_user(const char* user) override;

    /**
     * Handle an incoming request from the network
     */
    void handle_request(std::shared_ptr<Session>) override;

    /**
     * Handle local requests between installed mods
     * @param req request object
     */
    void handle_request(const fiy::fiy_request_t* req) override;
};
