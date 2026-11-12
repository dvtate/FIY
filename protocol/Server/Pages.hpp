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
    /// Pre-render in config data that will not change at runtime.
    static std::string pre_render_host_data(const std::string& templ) {
#define FIY_PROTOCOL_PAGES_GLOBAL_DATA(kv) \
            kv("domain", g_fiy->config.hostname) \
            kv("protocol", g_fiy->config.protocol)
        return MIN_SSR_MUSTACHE_VARIABLE_TEMPLATE(templ, FIY_PROTOCOL_PAGES_GLOBAL_DATA);
#undef FIY_PROTOCOL_PAGES_GLOBAL_DATA
    }

    /// Cache the template after substituting instance-wide values.
    template<const char* FileSubPath>
    static const std::string& file_contents() {
        static const std::string contents = pre_render_host_data(FileCache::get_file_contents<FileSubPath>());
        return contents;
    }

    // TODO Because navbars are context-sensitive, page templates should just have inline their own navbars like before.
    //      This provides more flexibility while avoiding complex logic and string copying.
    // TODO This also gave me an idea of allowing mods to have widgets.
    //      For example: notifications: unread count; contacts: pfp+dn

    /// Generate the navbar HTML
    static std::string navbar(const bool logged_in, const std::string_view current) {
        const std::string links = logged_in
            ? "<a href=\"/portal\">Portal</a>"
            : MinSSR::concat(
                "<a href=\"/portal\"",
                (current == "/portal" ? " class=\"current\" aria-current=\"page\"" : ""),
                ">Portal</a>",

                "<a href=\"/portal/login\"",
                (current == "/portal/login" ? " class=\"current\" aria-current=\"page\"" : ""),
                ">Log In</a>",

                "<a href=\"/portal/signup\"",
                (current == "/portal/signup" ? " class=\"current\" aria-current=\"page\"" : ""),
                ">Sign Up</a>");

        static constexpr char path[] = "navbar.html";
#define FIY_NAVBAR_RULES(kv) kv("navigation_links", links)
        return MIN_SSR_MUSTACHE(FileCache::get_file_contents<path>(), FIY_NAVBAR_RULES);
#undef FIY_NAVBAR_RULES
    }

    static Session::StringResponse signup_page(const unsigned status = 200, const std::string& err = "") {
        static constexpr char path[] = "signup.html";

        Session::StringResponse res;
        res.result(status);
        res.set(boost::beast::http::field::content_type, "text/html");

#define FIY_PROTOCOL_PAGES_SIGNUP_PAGE_RULES(kv) \
            kv("fail_reason", err) \
            kv("navbar", header)

        // All versions of the page are pre-rendered
        const auto header = navbar(false, "/portal/signup");
        res.body() = MIN_SSR_MUSTACHE_VARIABLE_TEMPLATE(file_contents<path>(), FIY_PROTOCOL_PAGES_SIGNUP_PAGE_RULES);
#undef FIY_PROTOCOL_PAGES_SIGNUP_PAGE_RULES

        return res;
    }

    static Session::StringResponse login_page(const unsigned status = 200, const std::string& err = "") {
        static constexpr char path[] = "login.html";

        Session::StringResponse res;
        res.result(status);
        res.set(boost::beast::http::field::content_type, "text/html");

#define FIY_PROTOCOL_PAGES_LOGIN_PAGE_RULES(kv) \
            kv("fail_reason", err) \
            kv("navbar", header)

        // All versions of the page are pre-rendered
        const auto header = navbar(false, "/portal/login");
        res.body() = MIN_SSR_MUSTACHE_VARIABLE_TEMPLATE(file_contents<path>(), FIY_PROTOCOL_PAGES_LOGIN_PAGE_RULES);
#undef FIY_PROTOCOL_PAGES_LOGIN_PAGE_RULES
        return res;
    }

    static Session::StringResponse portal_apps(const LocalUser& user) {
        static constexpr char path[] = "home.html";

        Session::StringResponse res;
        res.result(200);
        res.set(boost::beast::http::field::content_type, "text/html");
        res.set(boost::beast::http::field::cache_control, "private, no-store");

        const std::string user_json = user.json();
        const std::string mods_json = g_fiy->mods.get_mods_json();
        static const auto header = navbar(true, "/portal");

#define FIY_PROTOCOL_PAGES_PORTAL_APPS_RULES(kv) \
            kv("user_data", user_json) \
            kv("installed_apps", mods_json) \
            kv("navbar", header)
        res.body() = MIN_SSR_MUSTACHE(file_contents<path>(), FIY_PROTOCOL_PAGES_PORTAL_APPS_RULES);
#undef FIY_PROTOCOL_PAGES_PORTAL_APPS_RULES
        return res;
    }

    static std::string portal_settings(const LocalUser& user) {
        // Maybe do a settings mod instead?
        static constexpr char path[] = "settings.html";
#define FIY_PROTOCOL_PAGES_PORTAL_SETTINGS_RULES(kv) \
            kv("user_data", user.json()) \
            kv("navbar", header)
        const auto header = navbar(true, "/portal/settings");
        return MIN_SSR_MUSTACHE(file_contents<path>(), FIY_PROTOCOL_PAGES_PORTAL_SETTINGS_RULES);
#undef FIY_PROTOCOL_PAGES_PORTAL_SETTINGS_RULES
    }

    static Session::StringResponse default_landing_page() {
        static const char subpath[] = "index.html";

        Session::StringResponse res;
        res.result(200);
        res.set(boost::beast::http::field::content_type, "text/html");

#define FIY_PAGE_LANDING_RULES(kv) kv("navbar", header)
        // Result is always pre-rendered and cached
        const auto header = navbar(false, "/");
        res.body() = MIN_SSR_MUSTACHE_VARIABLE_TEMPLATE(file_contents<subpath>(), FIY_PAGE_LANDING_RULES);
#undef FIY_PAGE_LANDING_RULES
        return res;
    }

    // TODO move this to a 404.html file
    static Session::StringResponse not_found_page(const std::string& message = "The page you requested could not be found.") {
        Session::StringResponse res;
        res.result(404);
        res.set(boost::beast::http::field::content_type, "text/html");
        const auto header = navbar(false, "");
        const std::string safe_message = MinSSR::escape_html(message);
        res.body() = MinSSR::concat(
            "<!DOCTYPE html><html lang=\"en\"><head>"
            "<meta charset=\"UTF-8\">"
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
            "<title>404 Not Found &mdash; FIY</title>"
            "<link rel=\"stylesheet\" href=\"/portal/main.css\">"
            "<script src=\"/portal/theme.js\"></script>"
            "</head><body>",
            header,
            "<main><div class=\"page-head\"><h1>404 &mdash; Not found</h1>"
            "<p>" + safe_message + "</p>"
            "<div class=\"cta-row\">"
            "<a class=\"btn btn-primary\" href=\"/portal\">Back to portal</a>"
            "<a class=\"btn btn-secondary\" href=\"/\">About FIY</a>"
            "</div></div></main>"
            "<footer>FIY &mdash; Federate It Yourself.</footer>"
            "</body></html>"
        );
        return res;
    }
};
