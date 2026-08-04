//
// Created by tate on 12/5/25.
//

#pragma once

#include <utility>
#include <vector>
#include <string>
#include <string_view>
#include <functional>
#include <list>
#include <optional>
#include <boost/unordered/unordered_flat_map.hpp>

#include "../../../../util/RWMutex.hpp"

#include "../../../../modlib/fiymod.hpp"

#include "BasicRepo.hpp"
#include "GitRepo.hpp"
#include "../DTO/DTORepo.hpp"

struct RepoPageData;
// struct RepoFileBrowserPageData;

class LocalRepo : public BasicRepo, public GitRepo {
public:
    /// Short description for the repo
    std::string description{};

    /// Default read access (ie - view, clone, etc.)
    fiy::Locality visibility{};

    /// Is this repo a fork of another repo?
    std::string fork_of{};

    /// Local db id if available
    int id = -1;

    /// When was this repo created?
    time_t create_ts{-1};

    [[nodiscard]] bool valid() const {
        return create_ts != -1;
    }

    std::string to_json();
    bool from_json(const std::string& json);

    bool can_access(
        const Access min_level,
        const char* user,
        const char* domain = nullptr
    ) const;

    // TODO need structs for these so that we can make pages
    [[nodiscard]] ssize_t forks_count() const;
    ssize_t likes_count();
    ssize_t tickets_count();

    bool get_repo_page_data(const std::string& branch, RepoPageData& data);
    // bool get_repo_page_data(const std::string& branch, const std::string& path, RepoFileBrowserPageData& data);
    bool get_dto(const std::string& branch, DTORepo& dto);

protected:
    /// Used to create new repo
    LocalRepo() = default;

    // Can only have GitRepo instance per repository
    explicit LocalRepo(const BasicRepo& repo);

    ~LocalRepo();

    // TODO probably should make this static instance of generic class
    // TODO this should be in GitRepo, not here
    static RWMutex m_cache_mtx;
    static boost::unordered_flat_map<std::string, std::weak_ptr<LocalRepo>> m_cache;
    static std::vector<std::shared_ptr<LocalRepo>> m_cache_lru;
    static size_t m_lru_max_size;
    static std::mutex m_cache_lru_mutex;

    static void cache_lru_push(const std::shared_ptr<LocalRepo>& repo);

    const char* create();
public:
    /// Create a new repo
    static const char* create(const BasicRepo& basic_repo, std::string description, fiy::Locality visibility);

    static std::shared_ptr<LocalRepo> get_repo(const BasicRepo& repo);
    static std::vector<std::shared_ptr<LocalRepo>> get_repos(const std::vector<BasicRepo>& repos);
};
