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

    static Session::StringResponse signup_page(const unsigned status = 200, const std::string& err = "") {
        static constexpr char path[] = "signup.html";

        Session::StringResponse res;
        res.result(status);
        res.set(boost::beast::http::field::content_type, "text/html");

#define FIY_PROTOCOL_PAGES_SIGNUP_PAGE_RULES(kv) kv("fail_reason", err)
        // All versions of the page are pre-rendered
        res.body() = MIN_SSR_MUSTACHE_VARIABLE_TEMPLATE(file_contents<path>(), FIY_PROTOCOL_PAGES_SIGNUP_PAGE_RULES);
#undef FIY_PROTOCOL_PAGES_SIGNUP_PAGE_RULES

        return res;
    }

    static Session::StringResponse login_page(const unsigned status = 200, const std::string& err = "") {
        static constexpr char path[] = "login.html";

        Session::StringResponse res;
        res.result(status);
        res.set(boost::beast::http::field::content_type, "text/html");

        // All versions of the page are pre-rendered
#define FIY_PROTOCOL_PAGES_LOGIN_PAGE_RULES(kv) kv("fail_reason", err)
        res.body() = MIN_SSR_MUSTACHE_VARIABLE_TEMPLATE(file_contents<path>(), FIY_PROTOCOL_PAGES_LOGIN_PAGE_RULES);
#undef FIY_PROTOCOL_PAGES_LOGIN_PAGE_RULES
        return res;
    }

    static Session::StringResponse portal_apps(const LocalUser& user) {
        static constexpr char path[] = "home.html";

        Session::StringResponse res;
        res.result(200);
        res.set(boost::beast::http::field::content_type, "text/html");
        res.set(boost::beast::http::field::cache_control, "private"); // browser can cache but not CDNs

        const std::string user_json = user.json();
        const std::string mods_json = g_fiy->mods.get_mods_json();

#define FIY_PROTOCOL_PAGES_PORTAL_APPS_RULES(kv) \
            kv("user_data", user_json) \
            kv("installed_apps", mods_json)
        res.body() = MIN_SSR_MUSTACHE(file_contents<path>(), FIY_PROTOCOL_PAGES_PORTAL_APPS_RULES);
#undef FIY_PROTOCOL_PAGES_PORTAL_APPS_RULES
        return res;
    }

    static std::string portal_settings(const LocalUser& user) {
        // Maybe do a settings mod instead?
        static constexpr char path[] = "settings.html";
#define FIY_PROTOCOL_PAGES_PORTAL_SETTINGS_RULES(kv) kv("user_data", user.json())
        return MIN_SSR_MUSTACHE(file_contents<path>(), FIY_PROTOCOL_PAGES_PORTAL_SETTINGS_RULES);
#undef FIY_PROTOCOL_PAGES_PORTAL_SETTINGS_RULES
    }

    static Session::StringResponse default_landing_page() {
        static const char subpath[] = "index.html";
        Session::StringResponse res;
        res.result(200);
        res.set(boost::beast::http::field::content_type, "text/html");
        res.set(boost::beast::http::field::cache_control, "public, max-age=604800");
        res.body() = get_file_contents<subpath>();
        return res;
    }

    // TODO move this to a 404.html file
    static Session::StringResponse not_found_page(const std::string& message = "The page you requested could not be found.") {
        Session::StringResponse res;
        res.result(404);
        res.set(boost::beast::http::field::content_type, "text/html");
#define FIY_PROTOCOL_PAGES_404_RULES(kv) kv("message", MinSSR::escape_html(message))
        static constexpr char path[] = "error404.html";
        res.body() = MIN_SSR_MUSTACHE(file_contents<path>(), FIY_PROTOCOL_PAGES_404_RULES);
#undef FIY_PROTOCOL_PAGES_404_RULES
        return res;
    }
};
