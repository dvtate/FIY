//
// Created by tate on 3/3/26.
//

#pragma once

#include "Router.hpp"
#include "Pages.hpp"

/**
 * Respond to requests for static assets
 * @param path url subpath
 * @param req request object
 * @return true if routed, false otherwise
 */
inline bool static_asset_router(std::string_view path, const fiy::Request &req) {
    // Landing page (technically not static but whatever)
    if (path[0] == '/' && (path.size() == 1 || path[1] == '?')) {
        const auto body = Pages::landing_page(
            req.user == nullptr
                ? std::string("//") + fiy::host().domain + "/portal"
                : std::string(fiy::host().base_uri) + '/' + req.user);
        req.respond(200,
            "Content-Type: text/html; charset=utf-8\nCache-Control: max-age=604800",
            fiy::Body(body)
        );
        return true;
    }

    // stylesheet
    if (path == "/main.css") {
        static constexpr char file_path[] = "/main.css";
        req.respond(200,
            "Content-Type: text/css\nCache-Control: max-age=604800",
            Pages::mm_file_body<file_path>()
        );
        return true;
    }
    if (path == "/logo.svg") {
        static constexpr char file_path[] = "/logo.svg";
        req.respond(200,
            "Content-type: image/svg+xml\nCache-Control: max-age=604800",
            Pages::mm_file_body<file_path>());
        return true;
    }
    if (path == "/favicon.ico") {
        static constexpr char file_path[] = "/favicon.ico";
        req.respond(200,
            "Content-type: image/x-icon\nCache-Control: max-age=604800",
            Pages::mm_file_body<file_path>());
        return true;
    }

    // Fontawesome fonts
    if (path == "/fa/fa.css") {
        static constexpr char file_path[] = "font-awesome.css";
        req.respond(200,
            "Content-Type: text/css\nCache-Control: max-age=604800",
            Pages::mm_file_body<file_path>()
        );
        return true;
    }
    if (path.starts_with("/fonts/fontawesome-webfont.")) {
        path.remove_prefix(27);
        if (path.starts_with("eot")) {

            static constexpr char file_path[] = "fontawesome-webfont.eot";
            req.respond(200,
                "Content-Type: application/vnd.ms-fontobject\nCache-Control: max-age=604800",
                Pages::mm_file_body<file_path>()
            );
            return true;
        }
        if (path.starts_with("woff2")) {
            static constexpr char file_path[] = "fontawesome-webfont.woff2";
            req.respond(200,
                "Content-Type: font/woff2\nCache-Control: max-age=604800",
                Pages::mm_file_body<file_path>()
            );
            return true;
        }
        if (path.starts_with("woff")) {
            static constexpr char file_path[] = "fontawesome-webfont.woff";
            req.respond(200,
                "Content-Type: font/woff\nCache-Control: max-age=604800",
                Pages::mm_file_body<file_path>()
            );
            return true;
        }
        if (path.starts_with("ttf")) {
            static constexpr char file_path[] = "fontawesome-webfont.ttf";
            req.respond(200,
                "Content-Type: font/ttf\nCache-Control: max-age=604800",
                Pages::mm_file_body<file_path>()
            );
            return true;
        }
        if (path.starts_with("svg")) {
            static constexpr char file_path[] = "fontawesome-webfont.svg";
            req.respond(200,
                "Cache-Control: max-age=604800",
                Pages::mm_file_body<file_path>()
            );
            return true;
        }
    }

    return false;
}