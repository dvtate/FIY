//
// Created by tate on 5/28/26.
//
#include "Pages.hpp"

#include "../../../util/MinSSR.hpp"

std::string Pages::pre_render_host_data(const std::string& templ) {
#define FIY_MOD_GIT_HOST_DATA(kv) \
            kv("fiy_domain", fiy::host().domain) \
            kv("fiy_baseuri", fiy::host().base_uri)

    return MIN_SSR_MUSTACHE_VARIABLE_TEMPLATE(templ, FIY_MOD_GIT_HOST_DATA);
}

std::string Pages::repo_create_page(
    const std::string_view user,
    const std::vector<std::string>& orgs
) {
    std::string orgs_option_list;
    if (!orgs.empty()) {
        orgs_option_list += "<optgroup label=\"Organizations\">";
        for (const auto& org : orgs) {
            orgs_option_list += "<option value=\"";
            orgs_option_list += org;
            orgs_option_list += "\">";
            orgs_option_list += org;
            orgs_option_list += "</option>";
        }
        orgs_option_list += "</optgroup>";
    }

#define FIY_MOD_GIT_REPO_CREATE_RULES(kv) \
    kv("fiy_user", user) \
    kv("orgs_option_list", orgs_option_list) \
    kv("user_orgs_option_list", orgs_option_list)

    static constexpr char repo_create[] = "/repo_create.html";
    return MIN_SSR_MUSTACHE(file_contents<repo_create>(), FIY_MOD_GIT_REPO_CREATE_RULES);
}

std::string Pages::user_page(const std::string_view user, const char* request_user) {

#define USER_PAGE_RULES(kv) \
        kv("fiy_user", user) \
        kv("request_user", request_user == nullptr ? "" : request_user) \
        kv("fiy_display_name", user) \
        kv("fiy_user_bio", "")

    static constexpr char user_page[] = "/user.html";
    return MIN_SSR_MUSTACHE(file_contents<user_page>(), USER_PAGE_RULES);
}


std::string Pages::repo_page(const RepoPageData& repo, const char* request_user) {
    // TODO the right way to do this is probably to populate the page data by calling the API
    //      that we already have to implement in order for federation to work.
    static constexpr char repo_page[] = "/repo.html";
    auto pfp_url = [](const std::string& user) {
        return fiy::host().host_base_uri() + "/mods/contacts/pfp/" + user;
    };
    static constexpr std::string_view visibility_strs[] = {
        "Private", "Instance private", "Federated", "Public"
    };
    const bool empty = !repo.last_commit.valid();
    const std::string profile_link = request_user == nullptr ? ""
        : concat("<a href=\"", fiy::host().base_uri, '/', request_user, "\">Profile</a>");
    const auto count_str = [](const ssize_t count) {
        return count < 0 ? std::string("—") : std::to_string(count);
    };

#define FIY_MOD_GIT_REPO_PAGE_RULES(kv) \
        kv("repo_owner_pfp", pfp_url(repo.owner_user()) ) \
        kv("last_commit_author", repo.last_commit.author.profile_link() ) \
        kv("last_commit_time", time_str(repo.last_commit.ts) ) \
        kv("last_commit_time_diff", time_diff_str(fiy::host().now(), repo.last_commit.ts) ) \
        kv("last_commit_id_short", std::string_view(repo.last_commit.id).substr(0, 7) ) \
        kv("last_commit_msg", repo.last_commit.message.substr(0, repo.last_commit.message.find('\n')) ) \
        kv("mod_baseurl", fiy::host().base_uri ) \
        kv("repo_branches_count", count_str(repo.branches_count) ) \
        kv("repo_commits_count", count_str(repo.commits_count) ) \
        kv("repo_description", repo.description ) \
        kv("repo_name", repo.name ) \
        kv("repo_owner", repo.owner_user() ) \
        kv("repo_tags_count", count_str(repo.tags_count) ) \
        kv("repo_visibility", visibility_strs[(size_t)repo.visibility] ) \
        kv("repo_clone_url", fiy::host().base_uri + std::string("/") + repo.path() ) \
        kv("repo_entries_html", repo.entries_html() ) \
        kv("repo_empty_hidden", empty ? "" : "hidden" ) \
        kv("repo_files_hidden", empty ? "hidden" : "" ) \
        kv("profile_link", profile_link ) \
        kv("fiy_domain", fiy::host().domain )

    return MIN_SSR_MUSTACHE(file_contents<repo_page>(), FIY_MOD_GIT_REPO_PAGE_RULES);

    // TODO future: contributors, languages, readme viewer, repo_tags_html
}

std::string Pages::landing_page(const std::string_view profile_url) {
#define FIY_MOD_GIT_LANDING_PAGE_RULES(kv) kv("profile_url", profile_url)
    static constexpr char file_path[] = "/landing.html";
    return MIN_SSR_MUSTACHE(file_contents<file_path>(), FIY_MOD_GIT_LANDING_PAGE_RULES);
}

std::string Pages::error_page(
    const std::string_view title,
    const std::string_view description,
    const std::vector<std::pair<std::string, std::string>>& links
) {
    std::string links_str;
    for (const auto& [url, label] : links)
        links_str += concat("| <a href=\"" + url + "\">", label, "</a> ");

#define FIY_MOD_GIT_ERROR_PAGE_RULES(kv) \
        kv("error_title", title) \
        kv("error_description", description) \
        kv("links", links_str)
    static constexpr char file_path[] = "/error.html";
    return MIN_SSR_MUSTACHE(file_contents<file_path>(), FIY_MOD_GIT_ERROR_PAGE_RULES);
}
