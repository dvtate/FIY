//
// Created by tate on 2/27/26.
//

#pragma once

#include <string_view>

#include "../../../modlib/fiymod.hpp"

/**
 * Function that routes requests to correct handlers
 * @param path request subpath
 * @param req request object
 */
using RequestRouter = bool (*)(
    std::string_view path,
    const fiy::Request& req
);

void handle_request(const struct fiy::fiy_request_t* request);
