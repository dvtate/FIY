//
// Created by tate on 4/10/26.
//

#pragma once

#include "../../util/FileCache.hpp"

#include "../FIY.hpp"

#include "Session.hpp"
#include "../../util/MinSSR.hpp"

inline std::string fiy_portal_templates_dir() {
    return g_fiy->config.data_dir + "/pages/";
}

struct Pages : public FileCache<fiy_portal_templates_dir> {

    static std::string pre_render_host_data(const std::string& templ) {
#define FIY_PROTOCOL_PAGES_GLOBAL_DATA(kv) \
            kv("domain", g_fiy->config.hostname) \
            kv("protocol", "https://")
        return MIN_SSR_MUSTACHE_VARIABLE_TEMPLATE(templ, FIY_PROTOCOL_PAGES_GLOBAL_DATA);
#undef FIY_PROTOCOL_PAGES_GLOBAL_DATA
    }

    static std::string navbar(const LocalUser* user, const std::string_view current) {
        const auto link = [current](const std::string_view path, const std::string& label) {
            return "<a href=\"" + std::string(path) + "\""
                + (current == path ? " class=\"current\" aria-current=\"page\"" : "")
                + ">" + MinSSR::escape_html(label) + "</a>";
        };
        std::string links = link("/portal", user ? "Portal (" + user->username + ")" : "Portal");
        if (!user) {
            links += link("/portal/login", "Log In");
            links += link("/portal/signup", "Sign Up");
        }
        static constexpr char path[] = "navbar.html";
#define FIY_NAVBAR_RULES(kv) kv("navigation_links", links)
        return MIN_SSR_MUSTACHE(FileCache::get_file_contents<path>(), FIY_NAVBAR_RULES);
#undef FIY_NAVBAR_RULES
    }

    /// Cache the template after substituting instance-wide values.
    template<const char* FileSubPath>
    static const std::string& file_contents() {
        static const std::string contents = pre_render_host_data(FileCache::get_file_contents<FileSubPath>());
        return contents;
    }

    /// Substitute navigation into a cached page template.
    template<const char* FileSubPath>
    static std::string file_contents(const LocalUser* user, const std::string_view current = "") {
        const auto header = navbar(user, current);
#define FIY_PAGE_NAVBAR_RULES(kv) kv("navbar", header)
        return MIN_SSR_MUSTACHE(file_contents<FileSubPath>(), FIY_PAGE_NAVBAR_RULES);
#undef FIY_PAGE_NAVBAR_RULES
    }

    static Session::StringResponse signup_page(const unsigned status = 200, const std::string& err = "") {
        static constexpr char path[] = "signup.html";
        Session::StringResponse res;
        res.result(status);
        res.set(boost::beast::http::field::content_type, "text/html");
        const auto header = navbar(nullptr, "/portal/signup");
#define FIY_PROTOCOL_PAGES_SIGNUP_PAGE_RULES(kv) \
            kv("fail_reason", err) \
            kv("navbar", header)
        // All version of the page are pre-rendered
        res.body() = MIN_SSR_MUSTACHE_VARIABLE_TEMPLATE(file_contents<path>(), FIY_PROTOCOL_PAGES_SIGNUP_PAGE_RULES);
#undef FIY_PROTOCOL_PAGES_SIGNUP_PAGE_RULES
        return res;
    }
    static Session::StringResponse login_page(const unsigned status = 200, const std::string& err = "") {
        static constexpr char path[] = "login.html";
        Session::StringResponse res;
        res.result(status);
        res.set(boost::beast::http::field::content_type, "text/html");
        const auto header = navbar(nullptr, "/portal/login");
#define FIY_PROTOCOL_PAGES_LOGIN_PAGE_RULES(kv) \
            kv("fail_reason", err) \
            kv("navbar", header)
        // All version of the page are pre-rendered
        res.body() = MIN_SSR_MUSTACHE_VARIABLE_TEMPLATE(file_contents<path>(), FIY_PROTOCOL_PAGES_LOGIN_PAGE_RULES);
#undef FIY_PROTOCOL_PAGES_LOGIN_PAGE_RULES
        return res;
    }

    static Session::StringResponse portal_apps(const LocalUser& user) {
        Session::StringResponse res;
        res.result(200);
        res.set(boost::beast::http::field::content_type, "text/html");
        res.set(boost::beast::http::field::cache_control, "private, no-store");

        static const std::string mods_json = g_fiy->mods.get_mods_json();
        const std::string user_json = user.json();

#define FIY_PROTOCOL_PAGES_PORTAL_APPS_RULES(kv) \
            kv("user_data", user_json) \
            kv("installed_apps", mods_json) \
            kv("navbar", header)

        static constexpr char path[] = "home.html";
        const auto header = navbar(&user, "/portal");
        res.body() = MIN_SSR_MUSTACHE(file_contents<path>(), FIY_PROTOCOL_PAGES_PORTAL_APPS_RULES);
#undef FIY_PROTOCOL_PAGES_PORTAL_APPS_RULES
        return res;
    }

    static std::string portal_settings(const LocalUser& user) {
        static constexpr char path[] = "settings.html";
#define FIY_PROTOCOL_PAGES_PORTAL_SETTINGS_RULES(kv) \
            kv("user_data", user.json()) \
            kv("navbar", header)
        const auto header = navbar(&user, "");
        return MIN_SSR_MUSTACHE(file_contents<path>(), FIY_PROTOCOL_PAGES_PORTAL_SETTINGS_RULES);
#undef FIY_PROTOCOL_PAGES_PORTAL_SETTINGS_RULES
    }

    static Session::StringResponse not_found_page(const LocalUser* user, const std::string& message = "The page you requested could not be found.") {
        Session::StringResponse res;
        res.result(404);
        res.set(boost::beast::http::field::content_type, "text/html");
        const auto header = navbar(user, "");
        const std::string safe_message = MinSSR::escape_html(message);
        res.body() =
            "<!DOCTYPE html><html lang=\"en\"><head>"
            "<meta charset=\"UTF-8\">"
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
            "<title>404 Not Found &mdash; FIY</title>"
            "<link rel=\"preconnect\" href=\"https://fonts.googleapis.com\">"
            "<link rel=\"preconnect\" href=\"https://fonts.gstatic.com\" crossorigin>"
            "<link rel=\"stylesheet\" href=\"/portal/main.css?v=4\">"
            "<script src=\"/portal/theme.js?v=4\"></script>"
            "</head><body>"
            + header +
            "<main><div class=\"page-head\"><h1>404 &mdash; Not found</h1>"
            "<p>" + safe_message + "</p>"
            "<div class=\"cta-row\">"
            "<a class=\"btn btn-primary\" href=\"/portal\">Back to portal</a>"
            "<a class=\"btn btn-secondary\" href=\"/\">About FIY</a>"
            "</div></div></main>"
            "<footer>FIY &mdash; Federate It Yourself.</footer>"
            "</body></html>";
        return res;
    }
};
