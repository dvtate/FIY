//
// Created by tate on 4/30/26.
//

#pragma once

#include <vector>

#include "../Repos/GitRepo.hpp"
#include "DataTransferObject.hpp"

// TODO should probably nlohmann json's to_json/from_json functionality
//      but it's designed to use exceptions and it's not pretty so idk.
//      I'll just half-ass it for now.


/// Information that could vary by branch/subpath/ref/etc.
struct DTORepoTree : DataTransferObject {
    /// Last commit made to this tree
    GitRepo::Commit last_commit;

    /// What branch is this tree on
    std::string active_branch;

    /// Subpath within directory structure
    std::string path;

    /// Files, directories, etc. within this tree
    std::vector<GitRepo::Entry> entries;

    /// How many commits
    ssize_t commits_count{-1};

    std::string_view dto_class() final { return "repo.tree"; }
    bool from_json(const nlohmann::json& json) final;
    nlohmann::json to_json() final;
};

/// Basic repo info
struct DTORepoInfo : DataTransferObject {
    /// Instance of repo owner
    std::string instance;

    /// Username of the repo owner
    std::string owner;

    /// Repo name
    std::string name;

    /// User provided description for the repo
    std::string description;

    /// Is this repo a fork of another repo?
    std::string fork_of{};

    /// When was this repo created?
    time_t create_ts{0};

    /// Repo default visibility
    fiy::Locality visibility{fiy::Locality::USER};

    /// Default branch of the repo
    std::string default_branch;

    /// How many branches
    ssize_t branches_count{-1};

    /// How many likes?
    ssize_t likes_count{-1};

    /// How many tickets
    ssize_t tickets_count{-1};

    /// How many forks
    ssize_t forks_count{-1};

    /// How many tagged commits
    ssize_t tags_count{-1};

    std::string_view dto_class() final { return "repo.info"; }
    bool from_json(const nlohmann::json& json) final;
    nlohmann::json to_json() final;
};
