//
// Created by tate on 2/23/26.
//

#pragma once

#include <mutex>
#include <vector>
#include <git2.h>
#include <utility>

#include "BasicRepo.hpp"
#include "GitUser.hpp"

struct DTORepoTree;
struct RepoPageData;
struct RepoFileBrowserPageData;

/**
 * C++ wrapper for libgit2 operations on git repositories
 *
 * @remark for now this is only for local repos
 * @remark this class should only ever be instantiated by LocalRepo
 *  as duplicate instances for the same repo could be really bad
 */
class GitRepo {

protected:
    /// Git repository
    git_repository* m_repo{nullptr};

    /// Git revision walker
    /// @remark According to the docs, this is expensive to allocate and should be reused
    git_revwalk* m_walker{nullptr};

    /// Thread lock
    std::mutex m_mtx; // TODO replace with RWMutex

public:

    /// Git commit
    struct Commit {
        char id[GIT_OID_MAX_HEXSIZE + 1]{""};
        std::string message;
        time_t ts{0};
        GitUser author;
        GitUser committer;

        explicit Commit(git_commit* commit);
        Commit(git_commit* commit, git_mailmap* mailmap);
        Commit() = default;

        [[nodiscard]] bool valid() const {
            return ts != 0;
        }

        [[nodiscard]] std::string id_str() const {
            if (id[0] == '\0')
                return {};
            return std::string(this->id, GIT_OID_HEXSZ);
        }

        bool id_str(const std::string_view str) {
            if (str.size() >= GIT_OID_HEXSZ)
                return false;
            strncpy(this->id, str.data(), str.size());
            return true;
        }
    };

    /// Git tree entry
    struct Entry {
        enum Type {
            INVALID,
            COMMIT,
            DIRECTORY,
            FILE
            // Note: if new values are added they must go at the end
        } type = INVALID;
        std::string path;
        Commit last_commit;
    };

    GitRepo() = default;
    explicit GitRepo(const char* path);

    /**
     * Take ownership of a git repository
     * @param repo git repository object to take ownership of
     */
    explicit GitRepo(git_repository* repo): m_repo(repo) {}

    explicit GitRepo(const BasicRepo& repo);

    GitRepo(const GitRepo& other) = delete;
    GitRepo(GitRepo&& other)  noexcept {
        other.m_mtx.lock();
        m_repo = std::exchange( other.m_repo, nullptr);
        other.m_mtx.unlock();
    }

    ~GitRepo();

    bool open(const BasicRepo& repo);
    int create(const BasicRepo& repo, const char* description = nullptr);

    [[nodiscard]] git_repository* repo() const { return m_repo; }
    [[nodiscard]] bool is_open() const { return m_repo != nullptr; }

    // TODO should make these public/private depending on if they lock or not
    // [[nodiscard]] bool is_local() const { return m_local; }
    std::vector<std::string> tags();
    ssize_t tags_count();
    std::string default_branch();
    std::vector<std::string> branches();
    ssize_t branches_count();
    ssize_t commits_count(const std::string& branch);
    Commit last_commit(const std::string& branch);

    int entries(std::vector<Entry>& ret, const std::string& branch, const std::string& path = "");

    bool get_tree_dto_commit(DTORepoTree& dto, const std::string_view& commit, const std::string& path = "");
    bool get_tree_dto_branch(DTORepoTree& dto, const std::string& branch, const std::string& path = "");

protected:
    [[nodiscard]] git_revwalk* walker();
    int branch_tip(const std::string& branch, git_oid& oid);

    ssize_t commits_count(const git_oid* start_oid);
    Commit last_commit(const git_oid* start_oid);
    int entries(std::vector<Entry>& ret, const git_oid* target, const std::string& path = "");
    Commit last_commit(const git_oid* start_oid, const std::string& path, git_mailmap* mailmap);

    bool get_repo_page_data(const std::string& branch, RepoPageData& data);
    // bool get_dto(const std::string& branch, DTORepo& dto);

private:
    static bool ok(int status, const std::string& message= "");
};