//
// Created by tate on 2/23/26.
//

#pragma once

#include <string>

#include "../../../modlib/fiymod.hpp"

/**
 * Stores only the repository path and basic info
 */
struct BasicRepo {
    virtual ~BasicRepo() = default;

    enum class Access {
        Error = -1,
        Blocked = 0,
        Read = 1,
        Write = 2,
        Admin = 3,
        Owner = 4,
    };

    /// Instance of repo owner
    std::string instance;

    /// Username of the repo owner
    std::string owner;

    /// Repo name
    std::string name;

    BasicRepo() = default;
    BasicRepo(std::string owner, std::string name, std::string instance = ""):
        instance(std::move(instance)),
        owner(std::move(owner)),
        name(std::move(name))
    {}

    /// Owner username
    [[nodiscard]] std::string owner_user() const {
        std::string ret = this->owner;
        if (!this->instance.empty())
            ret += '@' + this->instance;
        return ret;
    }

    /// Get canonical path
    [[nodiscard]] std::string path() const {
        std::string ret = owner_user();
        ret += '/' + this->name;
        return ret;
    }

    [[nodiscard]] bool is_local() const {
        return this->instance.empty();
    }

    /// Where in the filesystem is this repo stored?
    [[nodiscard]] std::string fs_path() const {
        std::string ret = fiy::host().data_dir;
        ret += is_local() ? "/repos/" : "/mirrors/";
        ret += path();
        return ret;
    }

    bool from_path(std::string_view url);

    static std::string canonical_url(std::string url);

    bool can_access_local(
        const Access min_level,
        const char* user,
        const char* domain
    ) const;

    void http_cgi(const fiy::Request& req);

protected:
    bool user_can_access_local(
        const Access min_level,
        const char* user,
        const char* domain = nullptr
    ) const;
};