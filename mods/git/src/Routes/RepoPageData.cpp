//
// Created by tate on 2/26/26.
//

#include "RepoPageData.hpp"

#include <ctime>
#include <iomanip>

#include <nlohmann/json.hpp>

#include "Pages.hpp"

/**
 * Convert the entries list into HTML for the page
 */
std::string RepoFileBrowserPageData::entries_html() const {
    // This is probably really slow
    std::string ret;
    auto now = fiy::host().now();
    for (const auto& e : this->files) {
        ret += "<tr><td class=\"file-name\"><span class=\"file-icon\">";
        switch (e.type) {
            case GitRepo::Entry::DIRECTORY:
                ret += "\xf0\x9f\x93\x81"; // folder emoji
                break;
            case GitRepo::Entry::COMMIT:
                // TODO find something better
                ret += "\xf0\x9f\x93\x8d"; // pushpin emoji
                break;
            case GitRepo::Entry::FILE:
                ret += "\xf0\x9f\x93\x84"; // file emoji
                break;
            case GitRepo::Entry::INVALID:
                ret += "?";
                break;
        }
        ret += "</span> ";
        ret += e.path;
        ret += "</td><td class=\"file-commit-msg\">";
        ret += e.last_commit.message.substr(0,
            e.last_commit.message.find('\n'));
        ret += "</td><td class=\"file-time\" title=\"";
        ret += Pages::time_str(e.last_commit.ts);
        ret +="\">";
        ret += Pages::time_diff_str(now, e.last_commit.ts);
        ret += "</td></tr>\n";
    }
    return ret;
}

std::string RepoPageData::to_json() {
    auto ret = nlohmann::json::object();
    nlohmann::json::array_t entries;
    entries.reserve(this->files.size());
    for (const auto& [type, path, last_commit] : this->files) {
        auto o = nlohmann::json::object();
        o["commit"] = {
            { "id", std::string_view(last_commit.id) },
            { "message", last_commit.message },
            { "ts", last_commit.ts },
        };
        o["path"] = path;
        o["type"] = type;
        entries.emplace_back(std::move(o));
    }
    ret["entries"] = std::move(entries);

    auto lc = nlohmann::json::object();
    lc["ts"] = this->last_commit.ts;
    lc["message"] = this->last_commit.message;
    lc["id"] = std::string_view(this->last_commit.id);
    lc["author"] = {
        { "name", this->last_commit.author.name },
        { "email", this->last_commit.author.email },
        { "user", this->last_commit.author.canonical_user() },
    };
    lc["committer"] = {
        { "name", this->last_commit.committer.name },
        { "email", this->last_commit.committer.email },
        { "user", this->last_commit.committer.canonical_user() },
    };
    ret["last_commit"] = std::move(lc);

    ret["branch"] = this->active_branch;
    ret["default_branch"] = this->default_branch;
    ret["path"] = this->project_path;
    ret["visibility"] = this->visibility;

    ret["commits_count"] = this->commits_count;
    ret["tags_count"] = this->tags_count;
    ret["branches_count"] = this->branches_count;
    ret["likes_count"] = this->likes_count;
    ret["tickets_count"] = this->tickets_count;
    ret["forks_count"] = this->forks_count;

    ret["description"] = this->description;

    if (!this->fork_of.empty())
        ret["fork_of"] = this->fork_of;

    ret["create_ts"] = this->create_ts;

    return ret.dump();
}

bool RepoPageData::from_json(const std::string& json) {
    auto j = nlohmann::json::parse(json);

    // TODO
    return false;
}
