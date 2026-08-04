#include <deque>
#include <string>

#include "../../modlib/fiymod.hpp"


void handle_request(const struct fiy_request_t* request) {
    auto& req = *(const fiy::Request*) request;

    switch ((fiy::Request::Method)req.method) {
        case fiy::Request::Method::GET:
            
            break;

        case fiy::Request::Method::POST: {
            std::string username, password;
            // 
            bool ok = fiy::host().local_login(username.c_str(), password.c_str());
            break;
        };
        default:
            // 404
            break;
    }
}

FIY_EXPORT fiy::ModInfo* start(const fiy_host_info_t* host_info) {
    static fiy::ModInfo mod_info = {
        .on_request = handle_request,
        .delete_user = nullptr,
        .id = "fiy.oauth",
        .version = "0.0"
    };
    fiy::host() = *host_info;
    return &mod_info;
}
