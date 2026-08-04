//
// Created by tate on 7/5/24.
//

#include <iostream>

#include "fiymod.hpp"

static void handle_request(const fiy::fiy_request_t* _request) {
    const auto* req = (fiy::Request*) _request;

    std::string body_str = "<ul>";
    body_str += "<b>Method: </b>";
    body_str += req->method_str();
    body_str += "\n<br/>\n";
    body_str += "<b>Path: </b>";
    body_str += req->path;
    body_str += "\n<br/>\n";
    body_str += "<b>App: </b>";
    body_str += fiy::host().app_id;
    body_str += " @ ";
    body_str += fiy::host().domain;
    body_str += "\n<br/>\n";
    body_str += "<b>User: </b>";
    body_str += req->user_str("not logged in");
    body_str += "\n<br/>\n";
    body_str += "<b>Body: </b>";
    body_str += req->body;
    body_str += "</ul>\n<hr/>";

    printf("=====\nlib_cpp.so:\n%s : %s\nUser: %s\nDomain: %s\nBody: %s\nHeaders: %s\n=====\n",
           req->method_str(), req->path, req->user, req->domain,
           req->body, req->headers);

    req->respond(200,
        "Content-Type: text/html",
        fiy::Body(body_str));
}

//////////////////////////
// Exports
////////////////////////

FIY_EXPORT fiy::ModInfo* start(const fiy::fiy_host_info_t* host_info) {
    fiy::host() = *host_info;
    static fiy::ModInfo mod_info = {
        .on_request=handle_request,
        .delete_user = [](const char* username) {
            fiy::log_info(std::string("User deleted: ") + username);
        },
        .id = "example.cpp",
        .version = "0.0"
    };
    return &mod_info;
}

/// Export that gets called for the mod to do any cleanup before exit/dlclose
FIY_EXPORT void stop() {
    fiy::log_info("Mod stopped");
}