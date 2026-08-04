//
// Created by tate on 4/10/26.
//

#pragma once

#include "ModConnector.hpp"

/// Communicates with the module by dynamically linking
class ModConnectorDll : public ModConnector {

    void* m_dl_handle{nullptr};
    fiy::fiy_mod_info_t* m_mod_info{nullptr};

    struct ModDLLHostInfo;
    ModDLLHostInfo* m_host_info{nullptr};

public:
    ModConnectorDll(Mod* mod, std::string path): ModConnector(mod, std::move(path)) {}

    bool stop() override;
    bool start() override;
    void delete_user(const char* user) override;
    void handle_request(std::shared_ptr<Session> conn) override;
    void handle_request(const fiy::fiy_request_t* req) override;
};