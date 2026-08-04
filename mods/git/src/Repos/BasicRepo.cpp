//
// Created by tate on 2/23/26.
//


#include "BasicRepo.hpp"

#include <iostream>

#include "git_http_backend.hpp"
#include "../../../../modlib/fiymod.hpp"

#include "../DB.hpp"

using DB::operator ""_sql;

/**
 * Parse repo from request path
 * @param url repo string of form
 *      [/][@]user[@instance.xyz]/repo[.git][/][?][... things we don't care about]
 * @return false if parse failed
 */
bool BasicRepo::from_path(std::string_view url) {
    // shortest url: a/b
    if (url.size() < 3)
        return false;

    // Remove leading ////'s
    if (url[0] == '/')
        url = url.substr(url.find_first_not_of('/'));
    if (url.size() < 3)
        return false;

    // Remove leading @ to escape special usernames
    if (url[0] == '@')
        url = url.substr(1);
    if (url.size() < 3)
        return false;

    // Invalid
    auto slash = url.find('/');
    if (slash == std::string::npos)
        return false;

    // Get Instance
    const auto at = url.find('@');
    if (at == std::string::npos) {
        this->instance = "";
        this->owner = url.substr(0, slash);
    } else {
        // Invalid
        if (at > slash)
            return false;

        this->owner = url.substr(0, at);

        this->instance = url.substr(at + 1, slash - at - 1);
        if (instance == fiy::host().domain)
            this->instance = "";
    }

    // repo name shouldn't start with slash
    slash++;
    if (slash >= url.size())
        return false;

    // Ignore subpath or query strings
    auto end = url.find_first_of("/?", slash);
    if (end == std::string::npos)
        end = url.size();

    // Ignore .git
    if (url.compare(end - 4, 4, ".git") == 0)
        end -= 4;

    // Get repo name
    this->name = url.substr(slash, end - slash);

    //std::cout <<"BasicRepo::from_path: " <<this->owner << " @ " << this->instance << " / " <<this->name << "\n";
    return !this->name.empty();
}

/**
 * Check if the user has access to the repo
 * @param min_level minimum access level required
 * @param user username of relevant user
 * @param domain domain of relevant user
 * @remark this function should only be used when the repo is
 *  owned by our instance
 */
bool BasicRepo::can_access_local(
    const Access min_level,
    const char* user,
    const char* domain
) const {
    if (!this->is_local()) {
        // Proxy request to remote instance
        // if they don't have access, it will be blocked
        return true;
    }

    if (user != nullptr && this->owner == user)
        return min_level <= Access::Owner;

    // Check db
    if (min_level <= Access::Read) {
        // int64_t id = -1;
        int visibility = 0;
        thread_local auto q_repo = "SELECT id, visibility FROM Repos WHERE userName=? AND repoName=?"_sql;
        q_repo.bindNoCopy(1, this->owner);
        q_repo.bindNoCopy(2, this->name);

        // Query failed
        if (!q_repo.executeStep()) {
            std::cerr <<"DB: Could not find repo : " <<this->path() <<std::endl;
            q_repo.reset();
            return false;
        }

        // Get data from query
        // id = q_repo.getColumn(0).getInt64();
        visibility = q_repo.getColumn(1).getInt();
        q_repo.reset();

        switch (static_cast<fiy::Locality>(visibility)) {
            case fiy::Locality::PUBLIC:
                return true;
            case fiy::Locality::FEDIVERSE:
                return user != nullptr && domain != nullptr;
            case fiy::Locality::INSTANCE:
                return user != nullptr && domain == nullptr;
            case fiy::Locality::USER:
                return false;
            default:
                fiy::host().log_warning(
                    "Invalid visibility column value" + std::to_string(visibility));
        }
        return false;
    }

    // Impossible for unauthenticated user to get more than view permissions
    if (user == nullptr)
        return false;

    // Need more granular check.
    return user_can_access_local(min_level, user, domain);
}

/**
 * Check if the user has access using the RepoAccess table
 * @param min_level minimum access level required
 * @param user username of relevant user
 * @param domain domain of relevant user
 * @return true if user has requested access, false otherwise
 * @remark this function should only be used when the repo is
 *  owned by our instance
 */
bool BasicRepo::user_can_access_local(
    const Access min_level,
    const char* user,
    const char* domain
) const {
    // Need more granular check.
    thread_local auto q_access = "SELECT level FROM RepoAccess WHERE repoPath=? AND userName=?"_sql;
    q_access.bind(1, this->path());
    std::string user_str = user ? user : "";
    if (domain != nullptr) {
        user_str += '@';
        user_str += domain;
    }
    q_access.bind(2, user_str);

    while (q_access.executeStep())
        if (q_access.getColumn(0).getInt() >= (int) min_level) {
            q_access.reset();
            return true;
        }
    q_access.reset();
    return false;
}

/**
 * Convert repo path to cannonical form
 * @param url repo string of form
 *      [/]user[@instance.xyz]/repo[.git][/subpath/...][?query=sting]
 * @return canonical representation of form
 *          user[@remote.instance]/repo
 */
std::string BasicRepo::canonical_url(std::string url) {
    // shortest url: a/b
    if (url.size() < 3)
        return {};

    // Remove leading ////'s
    if (url[0] == '/')
        url = url.substr(url.find_first_not_of('/'));
    if (url.size() < 3)
        return {};

    // Invalid
    const auto slash = url.find('/');
    if (slash == std::string::npos)
        return {};

    // Remove local domain
    const auto at = url.find('@');
    if (at != std::string::npos) {
        // Invalid
        if (at > slash)
            return {};

        const auto instance = url.substr(at, slash - at);
        if (instance == fiy::host().domain) {
            // user@local/repo[.git] --> user/repo[.git]
            url = url.substr(0, at) + url.substr(slash);
        }
    }

    // Remove .git
    if (url.ends_with(".git"))
        url = url.substr(0, url.size() - 4);

    return url;
}

void BasicRepo::http_cgi(const fiy::Request& req) {
    git_repo_cgi(req);
}
