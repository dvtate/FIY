//
// Created by tate on 4/30/26.
//

#pragma once

#include "../../../../protocol/defs.hpp"
#include "../../../modlib/fiymod.hpp"

#include <nlohmann/json.hpp>

/**
 * Git commit author/commiter
 */
class GitUser {
public:
    std::string fiy_user;
    std::string name;
    std::string email;

    std::string profile_link() const;

    [[nodiscard]] std::string canonical_user() const {
        if (this->fiy_user.find('@', 1) == std::string::npos) {
            return this->fiy_user + '@' + fiy::host().domain;
        } else {
            return this->fiy_user;
        }
    }

    /// Sets the local user given a canonical user
    void set_user(const std::string_view canonical_user) {
        const auto i = canonical_user.find('@');
        if (i != std::string::npos
            && canonical_user.substr(i + 1) == fiy::host().domain
        )
            this->fiy_user = canonical_user.substr(0, i);
        else
            this->fiy_user = canonical_user;
    }
    void localize_user() {
        set_user(this->fiy_user);
    }
    bool has_user() const {
        return !this->fiy_user.empty();
    }
};

inline void to_json(nlohmann::json& j, const GitUser& user) {
    j = nlohmann::json::object();
    if (!user.name.empty())
        j["name"] = user.name;
    if (!user.email.empty())
        j["email"] = user.email;
    if (!user.has_user())
        j["user"] = user.canonical_user();
}

inline void from_json(const nlohmann::json& j, GitUser& user) {
    if (j.is_object()) [[likely]] {
        auto it = j.find("name");
        if (it != j.end())
            user.name = it->get<std::string>();
        it = j.find("email");
        if (it != j.end())
            user.email = it->get<std::string>();
        it = j.find("user");
        if (it != j.end())
            user.set_user(it->get<std::string>());
    } else {
        DEBUG_LOG("No value provided");
    }
}
