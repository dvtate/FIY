//
// Created by tate on 4/12/26.
//

#pragma once

#include "Router.hpp"

#include "../Repos/BasicRepo.hpp"

// Called by repo router
// For paths like /user/repo/ticket/:number

bool ticket_router(
    std::string_view path,
    const fiy::Request& req,
    const BasicRepo& repo
) {
    return false;
}