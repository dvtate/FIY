//
// Created by tate on 4/7/26.
//

#pragma once

#include "Pages.hpp"

inline bool user_router(std::string_view path, const fiy::Request& req) {
    const size_t start = path[0] == '/' ? 1 : 0;
    const auto end = path.find_first_of("/?#", start);
    std::string_view user;
    if (end == std::string_view::npos) {
        user = path.substr(start);
    } else if (path[end] == '/') {
        if (end == path.size() - 1 || path[end + 1] == '?' || path[end + 1] == '#')
            user = path.substr(start, end - start);
        else
            return false; // not a user -- more than one path component
    } else {
        user = path.substr(start, end - start);
    }

    // TODO maybe would be good to give a 404 when user is local and not found?

    const auto body = Pages::user_page(user, req.is_local() ? req.user : nullptr);
    req.respond(200,
        "Content-Type: text/html; charset=UTF-8\nCache-control: max-age=300",
        fiy::Body(body));
    return true;
}