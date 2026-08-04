//
// Created by tate on 7/5/24.
//

#pragma once

#include <string>
#include <memory>

#include "../defs.hpp"

#include "../../modlib/fiymod.h"

class Session;
class Mod;

/**
 * Abstract class that handles communication between the protocol host and mods
 */
class ModConnector {
protected:
    /// Owner
    Mod* m_mod{nullptr};

    /// Where can we connect to it?
    std::string m_uri;

public:
    ModConnector(Mod* mod, std::string path): m_mod(mod), m_uri(std::move(path)) {}

    // owning mod should call stop before destructor
    virtual ~ModConnector() = default;

    /// Initialize the app
    virtual bool start() = 0;

    /// Cleanly shut the app down
    virtual bool stop() = 0;

    /**
     * Handle request from the network
     * @param conn connection object
     * @remark this should return quickly so that we don't hang up event loop
     */
    virtual void handle_request(std::shared_ptr<Session> conn) = 0;

    /**
     * Handle a request from a local mod
     * @param req request object with callback + context
     */
    virtual void handle_request(const fiy::fiy_request_t* req) = 0;

    /// Handle user data deletion request
    virtual void delete_user(const char* user) = 0;

    const std::string& uri() const {
        return m_uri;
    }
};

