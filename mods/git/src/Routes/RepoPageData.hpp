//
// Created by tate on 2/26/26.
//

#pragma once

#include <string>
#include <vector>

#include "../Repos/LocalRepo.hpp"
#include "../Repos/GitRepo.hpp"

/**
 * Data shown for viewing files within a repo
 */
struct RepoFileBrowserPageData : public BasicRepo {
    RepoFileBrowserPageData() = default;

    /// List of files list
    std::vector<GitRepo::Entry> files;

    GitRepo::Commit last_commit;

    std::string active_branch;
    std::string default_branch;

    /// Project path we're looking at
    std::string project_path;

    fiy::Locality visibility{fiy::Locality::PUBLIC};

    [[nodiscard]] std::string entries_html() const;
};

/**
 * Data shown for repo landing page
 */
struct RepoPageData : public RepoFileBrowserPageData {

    RepoPageData() = default;
    ssize_t commits_count{-1};
    ssize_t tags_count{-1};
    ssize_t branches_count{-1};
    ssize_t likes_count{-1};
    ssize_t tickets_count{-1};
    ssize_t forks_count{-1};

    std::string description;

    /// Is this repo a fork of another repo?
    std::string fork_of;

    /// When was this repo created?
    time_t create_ts{-1};

    std::string to_json();
    bool from_json(const std::string& json);
};
