//
// Created by tate on 2/27/26.
//

#include <fstream>

#include <nlohmann/json.hpp>

#include "../../modlib/fiymod.hpp"

std::string g_proxy_target;

void handle_request(const fiy::Request& r) {
    // TODO the hard part: proxy request with added user header
    static const std::string body = "<h1>Not implemented</h1>"
        "<p>If you are an admin, please either update this mod or remove it</p>"
        "<p>This mod is configured to reverse-proxy requests to " + g_proxy_target
        + "</p>";
    r.respond(501, "Content-type: text/html", fiy::Body(body));
}

/// Export: Start module
FIY_EXPORT fiy::ModInfo* start(const fiy_host_info_t* host_info) {
    // Read config
    fiy::host() = *host_info;
    std::ifstream ifs{fiy::host().mod_config};
    auto config = nlohmann::json::parse(ifs);
    if (!(config.contains("mod_settings") && config.is_object()
        && (config = config["mod_settings"]).contains("target"))
    ) {
        fiy::log_fatal("missing required field: mod_settings.target: should be url to proxy");
        return nullptr;
    }
    if (!config["target"].is_string()) {
        fiy::log_fatal("mod_settings.target should be string containing url to proxy");
        return nullptr;
    }
    g_proxy_target = config["target"].get<std::string>();

    // Exchange info with host
    static fiy::ModInfo mod_info = {
        .on_request = [](const fiy_request_t* r) {
            handle_request(static_cast<const fiy::Request&>(*r));
        },
        .delete_user = nullptr
    };
    return &mod_info;
}