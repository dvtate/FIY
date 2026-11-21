//
// Created by tate on 2/23/26.
//

#pragma once

#include <string>

#include "../../../../util/MinSSR.hpp"
#include "../../../modlib/fiymod.hpp"

/**
 * Stores only the repository path and basic info
 */
struct BasicRepo {
    virtual ~BasicRepo() = default;

    /// Repo permissions
    enum class Access {
        /// Unable to determine permissions
        Error = -1,

        /// No access, repo is hidden
        Blocked = 0,

        /// Only read access to the repo
        Read = 1,

        /// Read + write access (ie - can push commits)
        Write = 2,

        /// Can change repo settings
        Admin = 3,

        /// All access, can delete, transfer ownership, etc.
        Owner = 4
    };

    /// Instance of repo owner
    /// - Empty when local
    std::string instance;

    /// Username of the repo owner
    /// - Owner must be a local user/org on the instance
    std::string owner;

    /// Repo name
    std::string name;

    BasicRepo() = default;
    // BasicRepo(std::string_view owner, std::string_view name) {
    //     auto at = owner.find('@');
    //     if (at != std::string_view::npos) {
    //         this->instance = owner.substr(at + 1);
    //         this->owner = owner.substr(0, at);
    //     } else {
    //         this->owner = owner;
    //         this->instance = "";
    //     }
    //     this->name = name;
    // }
    BasicRepo(std::string owner, std::string name, std::string instance = ""):
        instance(std::move(instance)),
        owner(std::move(owner)),
        name(std::move(name))
    {}

    /// Owner username
    [[nodiscard]] std::string owner_user(const bool canonical = false) const {
        std::string ret = this->owner;
        if (!this->instance.empty()) {
            ret += '@' + this->instance;
        } else if (canonical) {
            // Local
            ret += '@';
            ret += fiy::host().domain;
        }

        return ret;
    }

    /// Get canonical path
    [[nodiscard]] std::string path() const {
        return MinSSR::concat(owner_user(), '/', this->name);
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