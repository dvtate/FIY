//
// Created by tate on 2/27/26.
//

#pragma once

#include <deque>

#include "../../../util/WebUtils.hpp"

#include "../Repos/BasicRepo.hpp"
#include "../Repos/Repos.hpp"
#include "Pages.hpp"
#include "TicketRouter.hpp"

/// User is unauthenticated, send them to login page
static void unauthenticated(const fiy::Request& req) {
    if (req.user == nullptr) {
        // Anon/unauthenticated local user
        static const fiy::Response no_auth_resp{
            303,
            "Location: " + fiy::host().host_base_uri() + "/portal/login",
            fiy::Body()
        };
        req.respond(no_auth_resp);
    } else {
        // Unauthenticated remote API user
        req.respond(401);
    }
}

/// Show the new repo page
inline void repo_create_get(const fiy::Request& req) {
    if (req.user == nullptr) {
        unauthenticated(req);
        return;
    }
    if (req.domain != nullptr) {
        req.respond(401);
        return;
    }

    const std::string page = Pages::repo_create_page(req.user, {});
    req.respond(200,
        "Content-Type: text/html; charset=utf-8",
        fiy::Body(page)
    );
}

/// Handle repo create form action
inline void repo_create_post(const fiy::Request& req) {
    if (req.user == nullptr) {
        unauthenticated(req);
        return;
    }
    if (req.domain != nullptr) {
        // TODO this should eventually be allowed for orgs?
        req.respond(401);
        return;
    }

    // Parse form body
    std::deque<std::pair<std::string, std::string>> form;
    const bool ok = WebUtils::parse_form_url_encoded(
        std::string_view(req.body, req.body_len),
        form);
    if (!ok) {
        req.respond(400, "", "Invalid Form Body");
        fiy::log_info("Repo create: " + req.user_str() + " submitted invalid form body");
        return;
    }

    // Parse form data
    std::string description;
    BasicRepo repo;
    auto visibility = fiy::Locality::INSTANCE;
    for (auto& [k, v]: form) {
        if (k == "repo-owner") {
            while (v[0] == '@')
                v = v.substr(1);
            const auto at = v.rfind('@');
            if (at == std::string::npos) {
                repo.owner = v;
                continue;
            }
            repo.owner = v.substr(0, at);
            repo.instance = v.substr(at + 1);
            if (repo.instance == fiy::host().domain)
                repo.instance = "";
        } else if (k == "repo-name") {
            repo.name = v;
        } else if (k == "repo-description") {
            description = v;
        } else if (k == "repo-visibility") {
            visibility = (fiy::Locality) atoi(v.c_str()); // zero is failsafe
            if (visibility == fiy::Locality::USER && v != "0")
                fiy::log_info("Repo create: invalid visibility: " + v);
        } else {
            fiy::log_info("Repo create: " + req.user_str() + " gave invalid field: " + k);
        }
    }

    // Non-local!!! Needs to be created by peer
    if (repo.instance != "") {
        auto* remote_request = new fiy::Request(req);
        remote_request->domain = repo.instance.c_str();
        fiy::host().request_mod(fiy::host().app_id, remote_request, [remote_request, &req, repo](const fiy::Response* res) {
            if (res == nullptr) {
                const auto body = Pages::error_page(
                    "Peer Request Failed",
                    "Failed to create repo " + repo.path()
                    + " due to an unknown communication error with peer " + repo.instance
                    + ". You can safely try again. If " + repo.instance + " is not down and has not disabled federation"
                    " You may want to reach out to your instance's administrator or the peer's administrator in order"
                    " to figure out what's wrong.",
                    {
                        { fiy::host().base_uri + req.user_str(), "Profile" },
                        { fiy::host().base_uri + std::string("/repo/new"), "Create Repo" },
                    }
                );
                req.respond(500, "Content-Type: text/html", fiy::Body(body));
            } else if (res->status == 200) {
                // Send the user to the newly created repo
                const std::string body = concat(
                    "<meta http-equiv=\"refresh\" content=\"0; url=",
                    fiy::host().base_uri,
                    '/',
                    repo.path(),
                    "\" />"
                );
                req.respond(200, "Content-Type: text/html; charset=utf-8", body);
            } else {
                // Probably should parse and reformat for our own error page...
                req.respond(*res);
            }
            delete remote_request;
        });
    }

    // TODO org repos
    if (repo.owner != req.user) {
        req.respond(401);
        return;
    }

    const char* err = LocalRepo::create(repo, description, visibility);
    if (err != nullptr) {
        const auto body = Pages::error_page(
            "Failed to Create " + repo.path(),
            "Repo creation failed with the following message: " + std::string(err)
            + ". <br>" + (err[0] == '4'
                ? "You may want to try again with different options or make sure you have the necessary permissions."
                : "You may want to contact your instance administrator if errors persist."),
            {
                { fiy::host().base_uri + req.user_str(), "Profile" },
                { fiy::host().base_uri + std::string("/repo/new"), "Create Repo" },
            }
        );

        std::string msg = concat(
            "<h1>That didn't work!</h1><br/><p>",
            err,
            "</p>"
        );
        req.respond(err[0] == '4' ? 400 : 500,
            "Content-type: text/html; charset=utf-8",
            fiy::Body(body));
        return;
    }

    // Send the user to the newly created repo
    std::string body = concat(
        "<meta http-equiv=\"refresh\" content=\"0; url=",
        fiy::host().base_uri,
        '/' + repo.path(),
        "\" />"
    );
    req.respond(200, "Content-Type: text/html; charset=utf-8", body);
}

inline bool repo_router(
    std::string_view path,
    const fiy::Request& req
) {
    if (path.empty())
        return false;

    // Repo create
    if (path.starts_with("/repo")) {
        if (req.locality(fiy::host().domain) > fiy::Locality::INSTANCE) {
            unauthenticated(req);
            return true;
        }
        path.remove_prefix(5);
        if (path.starts_with("/new")) {
            if (req.method == fiy::Request::Method::GET) {
                repo_create_get(req);
                return true;
            }
            if (req.method == fiy::Request::Method::POST) {
                repo_create_post(req);
                return true;
            }
        }
        return false;
    }

    BasicRepo basic_repo;
    if (!basic_repo.from_path(path))
        return false;

    // Remove the /user/repo part
    std::string_view subpath = path;
    {
        // Skip leading /
        while (subpath[0] == '/')
            subpath.remove_prefix(1);

        // Skip user
        subpath.remove_prefix(subpath.find('/'));

        // Find end of repo name
        auto end = subpath.find('/', 1);
        if (end == std::string_view::npos)
            end = subpath.find_first_of("?#");
        if (end != std::string_view::npos)
            subpath.remove_prefix(end);
        else
            subpath = "";

        // Skip leading /
        while (subpath[0] == '/')
            subpath.remove_prefix(1);
    }

    // Handle remote repo
    if (!basic_repo.is_local()) {
        // TODO properly handle remote request
        // I think this only works for unauthenticated git pull
        fiy::host().log_info("remote request: " + std::string(path));
        auto* remote_request = new fiy::Request(req);
        remote_request->domain = basic_repo.instance.c_str();
        fiy::host().request_mod(fiy::host().app_id, remote_request,
            [req, remote_request](const fiy_response_t* res) {
                if (res == nullptr)
                    req.respond(500, "Content-type: text/html", "Peer error");
                else
                    req.respond(*res);
                delete remote_request;
            });
        return true;
    }

    // Handle local repo
    const auto repo = LocalRepo::get_repo(basic_repo);
    if (repo == nullptr || !repo->valid())
        return false;

    // Repo page
    auto access = LocalRepo::Access::Read;
    if (subpath.empty() || subpath[0] == '?' || subpath[0] == '#') {
        if (!repo->can_access(access, req.user, req.domain)) {
            unauthenticated(req);
            return true;
        }

        RepoPageData data;
        repo->get_repo_page_data(repo->default_branch(), data);
        const auto body = Pages::repo_page(data, req.domain == nullptr ? req.user : nullptr);
        req.respond(200, "Content-Type: text/html", fiy::Body(body));
        return true;
    }

    if (subpath.starts_with("commit/")) {
        subpath.remove_prefix(7);
        const auto commit_id = subpath.substr(0, subpath.find_first_of("/?#"));

        static const auto invalid_resp = fiy::Response{
            400,
            "",
            fiy::Body("Invalid commit id")
        };
        if (commit_id.size() > 100) {
            req.respond(invalid_resp);
            return true;
        }
        for (const char c : commit_id)
            if (! ((c >= 'a' && c <= 'f') || (c >= '0' && c <= '9'))) {
                req.respond(invalid_resp);
                return true;
            }

        // TODO Respond with output comparable to git show
        return false;
    }

    // Ticket router
    if (subpath.starts_with("tickets"))
        return ticket_router(path, req, basic_repo);

    // Else, we don't know the path, send it to the cgi
    if (std::string_view(req.path).find("git-receive-pack") != std::string_view::npos)
        access = LocalRepo::Access::Write;
    if (!repo->can_access(access, req.user, req.domain)) {

        // Need to send special header to git client
        if (req.find_header("User-Agent").starts_with("git/")) {
            req.respond(401,
                "WWW-Authenticate: Basic charset=\"UTF-8\"");
            return true;
        }
        unauthenticated(req);
        return true;
    }

    // Paths:
    // /user/repo               -- summary page (default branch)
    // /user/repo/tickets       -- issues/pr tab
    // /user/repo/settings      -- settings page (must check permissions)

    // we don't know what to do, pass it on to the cgi
    repo->http_cgi(req);

    return true; // we have to be called last
}
