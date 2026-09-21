//
// Created by tate on 2/23/26.
//

#include "GitRepo.hpp"

#include <unordered_set>

#include "../../../../util/FileCache.hpp"

#include "../../../../modlib/fiymod.hpp"

#include "../Routes/RepoPageData.hpp"

/**
 * Check for git error
 * @param status git error
 * @param message what did the error stop
 * @return true if no error, false otherwise
 */
bool GitRepo::ok(const int status, const std::string& message) {
    if (status != GIT_OK) [[unlikely]] {
        const git_error* e = git_error_last();
        std::string log_msg = message;
        log_msg += ": ";
        log_msg += e != nullptr ? e->message : "Unknown error";
        fiy::log_error(log_msg);
        return false;
    }
    return true;
}

GitRepo::GitRepo(const char* path) {
    std::lock_guard lock{m_mtx};
    ok( git_repository_open(&m_repo, path),
        "Failed to open repo " + std::string(path));
}

GitRepo::GitRepo(const BasicRepo& repo):
    GitRepo(repo.fs_path().c_str())
{}

GitRepo::~GitRepo() {
    m_mtx.lock();
    if (m_repo != nullptr) {
        git_repository_free(m_repo);
        m_repo = nullptr;
    }
    if (m_walker != nullptr) {
        git_revwalk_free(m_walker);
        m_walker = nullptr;
    }
    m_mtx.unlock();
}

/**
 * Get git_revwalk instance
 * @remark m_mtx must be locked by caller
 * @return git_revwalk instance
 */
git_revwalk* GitRepo::walker() {
    if (m_walker == nullptr)
        ok(
            git_revwalk_new(&m_walker, m_repo),
            "git_revwalk_new()"
        );
    return m_walker;
}

bool GitRepo::open(const BasicRepo& repo) {
    std::lock_guard lock{m_mtx};
    std::string repo_path = concat(
        fiy::host().data_dir,
        repo.is_local() ? "/repos/" : "/mirrors/",
        repo.path()
    );
    return ok(
        git_repository_open(&m_repo, repo_path.c_str()),
        "Failed to open repo" + repo.path()
    );
}

/**
 * Create a new bare repo
 * @param repo
 * @param description
 * @return
 */
int GitRepo::create(const BasicRepo& repo, const char* description) {
    std::lock_guard lock{m_mtx};

    // Construct path
    std::string repo_path = concat(
        fiy::host().data_dir,
        repo.is_local() ? "/repos/" : "/mirrors/",
        repo.path()
    );

    // Set options for a bare repository
    git_repository_init_options opts = GIT_REPOSITORY_INIT_OPTIONS_INIT;
    opts.flags = GIT_REPOSITORY_INIT_BARE;
    std::string origin_url;
    if (!repo.is_local()) {
        origin_url = "https://" + std::string(repo.instance)
            + "/" + repo.owner + "/" + repo.name;
        opts.origin_url = origin_url.c_str();
    }
    opts.description = description; // Optional: set a description

    // Create the repository
    const int error = git_repository_init_ext(&m_repo, repo_path.c_str(), &opts);
    if (error < 0) {
        const git_error *e = giterr_last();
        fiy::log_error("Failed to create bare repo "
            + repo.path() + ": " + std::string(e->message));
    } else {
        fiy::log_info("Created repo: " + repo.path());
    }

    // TODO if not local, fetch from remote

    return error;
}

/**
 * Get the tags
 * @return vector containing all tags for the repo
 */
std::vector<std::string> GitRepo::tags() {
    std::lock_guard lock{m_mtx};
    std::vector<std::string> ret;
    git_strarray tags;
    if (!ok(git_tag_list(&tags, m_repo), "Failed to list tags"))
        return {};
    ret.reserve(tags.count);
    for (size_t i = 0; i < tags.count; ++i)
        ret.emplace_back(tags.strings[i]);
    git_strarray_dispose(&tags);
    return ret;
}

/**
 * Get the number of tags in the repo
 * @return number of tags in the repo or -1 on error
 */
ssize_t GitRepo::tags_count() {
    std::lock_guard lock{m_mtx};
    git_strarray tags;
    if (!ok(git_tag_list(&tags, m_repo), "Failed to list tags"))
        return -1;
    const auto ret = static_cast<ssize_t>(tags.count);
    git_strarray_dispose(&tags);
    return ret;
}

std::vector<std::string> GitRepo::branches() {
    std::lock_guard lock{m_mtx};
    std::vector<std::string> ret;
    git_branch_iterator* it = nullptr;
    if (!ok(
        git_branch_iterator_new(&it, m_repo, GIT_BRANCH_LOCAL),
        "Failed to create branch iterator"
    ))
        return {};

    git_reference* ref;
    git_branch_t type;
    while (git_branch_next(&ref, &type, it) == 0) {
        const char* name = nullptr;
        git_branch_name(&name, ref);
        ret.emplace_back(name);
        git_reference_free(ref);
    }

    git_branch_iterator_free(it);
    return ret;
}

ssize_t GitRepo::branches_count() {
    std::lock_guard lock{m_mtx};
    git_branch_iterator* it = nullptr;
    if (!ok(git_branch_iterator_new(&it, m_repo, GIT_BRANCH_ALL)))
        return -1;

    ssize_t count = 0;
    git_reference* ref;
    git_branch_t type;
    while (git_branch_next(&ref, &type, it) == 0) {
        ++count;
        git_reference_free(ref);
    }

    git_branch_iterator_free(it);
    return count;
}

/**
 * Get the default branch name
 * @return default branch name or "" on error
 */
std::string GitRepo::default_branch() {
    std::lock_guard lock{m_mtx};
    git_reference* head = nullptr;
    std::string ret;
    if (ok(git_repository_head(&head, m_repo))) {
        const char* branch = git_reference_shorthand(head);
        ret = branch;
        git_reference_free(head);
    }
    return ret;
}

/**
 * Get the commit tip for the specified branch
 * @param branch (in) branch to get tip of
 * @param oid (out)
 * @remark m_mtx should be locked before calling this member
 * @return git error
 */
int GitRepo::branch_tip(const std::string& branch, git_oid& oid) {
    git_reference* ref = nullptr;
    int status = git_branch_lookup(&ref, m_repo, branch.c_str(), GIT_BRANCH_ALL);
    if (status < 0)
        return status;

    const git_oid* target = git_reference_target(ref);
    if (target == nullptr) {
        git_reference_free(ref);
        return -1;
    }
    oid = *target;
    git_reference_free(ref);
    return 0;
}

/**
 * Count the commits since a starting point
 * @param start_oid commit to start from
 * @return count of commits
 * @remark m_mtx should be locked before calling this function
 */
ssize_t GitRepo::commits_count(const git_oid* start_oid) {
    git_revwalk* walk = walker();

    git_revwalk_sorting(walk, GIT_SORT_TIME | GIT_SORT_TOPOLOGICAL);
    git_revwalk_push(walk, start_oid);

    ssize_t count = 0;
    git_oid oid;
    while (git_revwalk_next(&oid, walk) == 0)
        ++count;

    git_revwalk_reset(walk);
    return count;
}

/**
 * get total number of commits from a starting commit oid
 * @param branch branch to use
 * @return total number of commits
 */
ssize_t GitRepo::commits_count(const std::string& branch) {
    std::lock_guard lock(m_mtx);
    git_oid target;
    if (!ok(branch_tip(branch, target)))
        return -1;
    return commits_count(&target);
}

GitRepo::Commit::Commit(git_commit* commit, git_mailmap* mailmap) {
    // Invalid
    if (commit == nullptr) {
        this->ts = 0;
        return;
    }

    const git_oid* oid = git_commit_id(commit);
    git_oid_tostr(this->id, sizeof(this->id), oid);

    this->message = git_commit_message(commit);
    this->ts = git_commit_time(commit);

    // Get names+emails
    //  The mailmap can provide emails -> FIY user mapping
    git_signature* mm_sig = nullptr;
    if (    mailmap != nullptr
        &&  ok(git_commit_author_with_mailmap(&mm_sig, commit, mailmap))
        &&  mm_sig != nullptr
    ) {
        if (mm_sig->name)
            this->author.name = mm_sig->name;
        if (mm_sig->email)
            this->author.email = mm_sig->email;
        git_signature_free(mm_sig);
    } else {
        // Failed to get mailmap signature, get it from commit instead
        const git_signature* sig = git_commit_author(commit);
        if (sig != nullptr) {
            if (sig->name != nullptr)
                this->author.name = sig->name;
            if (sig->email != nullptr)
                this->author.email = sig->email;
        }
    }
    mm_sig = nullptr;

    if (mailmap != nullptr
        && ok(git_commit_committer_with_mailmap(&mm_sig, commit, mailmap))
        && mm_sig != nullptr
    ) {
        if (mm_sig->name)
            this->committer.name = mm_sig->name;
        if (mm_sig->email)
            this->committer.email = mm_sig->email;
        git_signature_free(mm_sig);
    } else {
        const git_signature* sig = git_commit_committer(commit);
        if (sig != nullptr) {
            if (sig->name != nullptr)
                this->committer.name = sig->name;
            if (sig->email != nullptr)
                this->committer.email = sig->email;
        }
    }

    // Get fiy user from the commit notes
    git_note* note = nullptr;
    git_note_commit_read(&note, git_commit_owner(commit), commit, oid);
    if (note != nullptr) {
        std::string_view notes = git_note_message(note);
        constexpr auto author_key = "FIY-author: ";
        auto start = notes.find(author_key);
        if (start != std::string_view::npos) {
            const auto end = notes.find('\n', start + strlen(author_key));
            if (end != std::string_view::npos)
                this->author.set_user(notes.substr(start, end - start));
            else
                this->author.set_user(notes.substr(start));
        }

        constexpr auto committer_key = "FIY-committer: ";
        start = notes.find(committer_key);
        if (start != std::string_view::npos) {
            const auto end = notes.find('\n', start + strlen(committer_key));
            if (end != std::string_view::npos)
                this->committer.set_user(notes.substr(start, end - start));
            else
                this->committer.set_user(notes.substr(start));
        }
        git_note_free(note);
    }

    // Default to using email
    if (!this->author.has_user())
        this->author.set_user(this->author.email);
    if (!this->committer.has_user())
        this->committer.set_user(this->committer.email);
}

GitRepo::Commit::Commit(git_commit* commit) {
    git_mailmap* mailmap = nullptr;
    if (commit != nullptr)
        if (!ok(git_mailmap_from_repository(&mailmap, git_commit_owner(commit))))
            mailmap = nullptr;
    *this = Commit(commit, mailmap);
    git_mailmap_free(mailmap);
}

/**
 * Describe the most recent commit
 * @param start_oid commit to look up
 * @return commit object
 * @remark m_mtx should be locked before calling this function
 */
GitRepo::Commit GitRepo::last_commit(const git_oid* start_oid) {
    git_commit* tip = nullptr;
    if (git_commit_lookup(&tip, m_repo, start_oid) < 0)
        return Commit(nullptr);

    Commit ret{tip};
    git_commit_free(tip);
    return ret;
}

/**
 * Describe most recent commit for a given branch
 * @param branch name of the branch to check
 * @return commit object
 */
GitRepo::Commit GitRepo::last_commit(const std::string& branch) {
    std::lock_guard lock(m_mtx);
    git_oid target;
    if (!ok(branch_tip(branch, target)))
        return Commit(nullptr);
    return last_commit(&target);
}

/**
 * Find last commit affecting given path
 * @param start_oid commit to start search at
 * @param path path to the file/tree entry
 * @return commit object
 */
GitRepo::Commit GitRepo::last_commit(
    const git_oid* start_oid,
    const std::string& path,
    git_mailmap* mailmap
) {
    git_revwalk* walk = walker();
    git_revwalk_sorting(walk, GIT_SORT_TIME);
    git_revwalk_push(walk, start_oid);

    git_oid oid;
    while (git_revwalk_next(&oid, walk) == 0) {
        git_commit* commit = nullptr;
        if (git_commit_lookup(&commit, m_repo, &oid) < 0)
            continue;

        // Root commit (ie - "git init")
        if (git_commit_parentcount(commit) == 0) {
            Commit ret{commit, mailmap};
            git_commit_free(commit);
            git_revwalk_reset(walk);
            return ret;
        }

        git_commit* parent = nullptr;
        git_commit_parent(&parent, commit, 0); // should we check all the parents?

        git_tree *ctree = nullptr, *ptree = nullptr;
        git_commit_tree(&ctree, commit);
        git_commit_tree(&ptree, parent);

        git_diff* diff = nullptr;
        git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
        const char* pathspec = path.c_str();
        git_strarray arr{ const_cast<char**>(&pathspec), 1 };
        opts.pathspec = arr;

        git_diff_tree_to_tree(&diff, m_repo, ptree, ctree, &opts);

        if (git_diff_num_deltas(diff) > 0) {
            Commit ret{commit, mailmap};

            git_diff_free(diff);
            git_tree_free(ctree);
            git_tree_free(ptree);
            git_commit_free(parent);
            git_commit_free(commit);
            git_revwalk_reset(walk);
            return ret;
        }

        git_diff_free(diff);
        git_tree_free(ctree);
        git_tree_free(ptree);
        git_commit_free(parent);
        git_commit_free(commit);
    }

    git_revwalk_reset(walk);
    return Commit(nullptr);
}

int GitRepo::entries(
    std::vector<GitRepo::Entry>& ret,
    const std::string& branch,
    const std::string& path
) {
    std::lock_guard lock(m_mtx);
    git_oid target;
    const int status = branch_tip(branch, target);
    if (status != GIT_OK)
        return status;
    return entries(ret, &target, path);
}

int GitRepo::entries(
    std::vector<GitRepo::Entry>& ret,
    const git_oid* target,
    const std::string& path
) {
    // Get tree from commit
    git_commit* commit = nullptr;
    int status = git_commit_lookup(&commit, m_repo, target);
    if (status != GIT_OK)
        return status;
    git_tree* tree = nullptr;
    status = git_commit_tree(&tree, commit);
    if (status != GIT_OK) {
        git_commit_free(commit);
        return status;
    }

    // TODO only get entries in path

    // Get entry paths
    const size_t n = git_tree_entrycount(tree);
    for (size_t i = 0; i < n; ++i) {
        const git_tree_entry* entry = git_tree_entry_byindex(tree, i);

        Entry e;
        const char* name = git_tree_entry_name(entry);
        e.path = name ? name : "";

        switch (git_tree_entry_type(entry)) {
            case GIT_OBJECT_TREE:
                e.type = Entry::DIRECTORY;
                break;

            case GIT_OBJECT_COMMIT:
            case GIT_OBJECT_TAG: {
                // char sub_oid[GIT_OID_MAX_HEXSIZE + 1];
                // git_oid_tostr(sub_oid, sizeof(sub_oid),
                //               git_tree_entry_id(entry));
                // e.path += " @ ";
                // e.path += sub_oid;
                e.type = Entry::COMMIT;
                break;
            }

            case GIT_OBJECT_BLOB:
            default:
                e.type = Entry::FILE;
                break;
        }
        ret.emplace_back(e);
    }

    // Get entry last commits
    // TODO would be better to do only one revwalk and update files all at once
    git_mailmap* mailmap;
    (void)ok(git_mailmap_from_repository(&mailmap, m_repo));
    for (auto& e : ret)
        e.last_commit = last_commit(target, e.path, mailmap);

    git_mailmap_free(mailmap);
    git_tree_free(tree);
    git_commit_free(commit);
    return 0;
}

bool GitRepo::get_repo_page_data(const std::string& branch, RepoPageData& data) {
    // No lock required
    data.active_branch = branch;

    // These lock themselves
    data.default_branch = default_branch();
    data.branches_count = branches_count();
    data.tags_count = tags_count();

    // These expect the mutex to be locked already
    std::lock_guard lock{m_mtx};
    git_oid target;
    if (!ok(branch_tip(branch, target), "branch_tip"))
        return false;

    data.commits_count = commits_count(&target);
    data.last_commit = last_commit(&target);
    if (!ok(entries(data.files, &target), "entries"))
        return false;

    return true;
}

bool GitRepo::get_tree_dto_branch(DTORepoTree& dto, const std::string& branch, const std::string& path) {
    dto.active_branch = branch;
    dto.path = path;

    std::lock_guard lock{m_mtx};
    git_oid target;
    if (!ok(branch_tip(dto.active_branch, target), "branch_tip"))
        return false;

    dto.commits_count = commits_count(&target);
    dto.last_commit = last_commit(&target);
    if (!ok(entries(dto.entries, &target), "entries"))
        return false;
    return true;
}

bool GitRepo::get_tree_dto_commit(DTORepoTree& dto, const std::string_view& commit, const std::string& path) {
    dto.path = path;

    std::lock_guard lock{m_mtx};
    git_oid target;
    if (!ok(git_oid_fromstrn(&target, commit.data(), commit.size())))
        return false;

    dto.commits_count = commits_count(&target);
    dto.last_commit = last_commit(&target);
    if (!ok(entries(dto.entries, &target), "entries"))
        return false;
    dto.active_branch = std::string(commit);
    return true;
}
