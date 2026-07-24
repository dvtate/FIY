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

    static std::string pre_render_host_data(const std::string templ) {
#define FIY_PROTOCOL_PAGES_GLOBAL_DATA(kv) \
            kv("domain", g_fiy->config.hostname) \
            kv("protocol", "https://")
        return MIN_SSR_MUSTACHE_VARIABLE_TEMPLATE(templ, FIY_PROTOCOL_PAGES_GLOBAL_DATA);
#undef FIY_PROTOCOL_PAGES_GLOBAL_DATA
    }

    /// Automatically replace host_data in the loaded template
    template<const char* FileSubPath>
    static std::string file_contents() {
        return pre_render_host_data(
            FileCache::get_file_contents<FileSubPath>());
    }

    static Session::StringResponse signup_page(const unsigned status = 200, const std::string& err = "") {
        static constexpr char path[] = "signup.html";
        Session::StringResponse res;
        res.result(status);
        res.set(boost::beast::http::field::content_type, "text/html");
#define FIY_PROTOCOL_PAGES_SIGNUP_PAGE_RULES(kv) kv( "fail_reason", err)
        // All version of the page are pre-rendered
        res.body() =  MIN_SSR_MUSTACHE_VARIABLE_TEMPLATE(file_contents<path>(), FIY_PROTOCOL_PAGES_SIGNUP_PAGE_RULES);
#undef FIY_PROTOCOL_PAGES_SIGNUP_PAGE_RULES
        return res;
    }
    static Session::StringResponse login_page(const unsigned status = 200, const std::string& err = "") {
        static constexpr char path[] = "login.html";
        Session::StringResponse res;
        res.result(status);
        res.set(boost::beast::http::field::content_type, "text/html");
#define FIY_PROTOCOL_PAGES_LOGIN_PAGE_RULES(kv) kv( "fail_reason", err)
        // All version of the page are pre-rendered
        res.body() =  MIN_SSR_MUSTACHE_VARIABLE_TEMPLATE(file_contents<path>(), FIY_PROTOCOL_PAGES_LOGIN_PAGE_RULES);
#undef FIY_PROTOCOL_PAGES_LOGIN_PAGE_RULES
        return res;
    }

    static Session::StringResponse portal_apps(const LocalUser& user) {
        Session::StringResponse res;
        res.result(200);
        res.set(boost::beast::http::field::content_type, "text/html");

        static const std::string mods_json = g_fiy->mods.get_mods_json();
        const std::string user_json = user.json();

#define FIY_PROTOCOL_PAGES_PORTAL_APPS_RULES(kv) \
            kv("user_data", user_json) \
            kv("installed_apps", mods_json)

        static constexpr char path[] = "home.html";
        res.body() = MIN_SSR_MUSTACHE(file_contents<path>(), FIY_PROTOCOL_PAGES_PORTAL_APPS_RULES);
#undef FIY_PROTOCOL_PAGES_PORTAL_APPS_RULES
        return res;
    }

    static std::string portal_settings(const LocalUser& user) {
        static constexpr char path[] = "settings.html";
#define FIY_PROTOCOL_PAGES_PORTAL_SETTINGS_RULES(kv) kv("user_data", user.json())
        return MIN_SSR_MUSTACHE(file_contents<path>(), FIY_PROTOCOL_PAGES_PORTAL_SETTINGS_RULES);
#undef FIY_PROTOCOL_PAGES_PORTAL_SETTINGS_RULES
    }
};
