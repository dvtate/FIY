//
// Created by tate on 4/30/26.
//

#include "Router.hpp"

#include "ApiRouter.hpp"
#include "Pages.hpp"
#include "AssetRouter.hpp"
#include "RepoRouter.hpp"
#include "UserRouter.hpp"

/**
 * Request handler
 * @param request incoming user request from host
 * @param cb callback from host
 */
void handle_request(const struct fiy::fiy_request_t* request) {
    auto& req = *(fiy::Request*)request;

    std::string_view path = req.path;

    if ( static_asset_router(path, req)
        || user_router(path, req)
        || api_router(path, req)
        || repo_router(path, req)
    )
        return;

    // No router
    // TODO custom 404 page?
    req.respond(404, "", "Not Found");
}
