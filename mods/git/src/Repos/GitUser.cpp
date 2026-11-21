//
// Created by tate on 4/30/26.
//

#include "GitUser.hpp"

#include <format>

#include "../../../../util/FileCache.hpp"

/**
 * @return HTML for a link to user's profile or at least a titled span
 */
std::string GitUser::profile_link() const {
    // FIY user: profile link
    if (!fiy_user.empty())
        return concat(
            "<a href=\"", fiy::host().base_uri, '/', fiy_user, "\">",
            fiy_user, "</a>"
        );

    // Otherwise: name + email
    std::string ret = "<span title=\"";
    if (name.empty()) {
        ret += email.empty() ? "unknown" : email;
    } else {
        ret += name;
        if (!email.empty()) {
            ret += " <";
            ret += email;
            ret += '>';
        }
    }
    ret += "\">";
    ret += name.empty()
        ? (email.empty() ? "<unknown>" : email)
        : name;
    ret += "</span>";
    return ret;
}
