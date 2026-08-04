//
// Created by tate on 7/30/25.
//

#include <cstdint>
#include <string_view>
#include <utility>

#include "../../../third_party/base64.hpp"

#include "../../../modlib/fiymod.hpp"

#include "DB.hpp"
#include "Pages.hpp"
#include "Contact.hpp"
#include "timezones.hpp"

/**
 * User profile endpoint
 * @param user_str user to get profile of
 * @param req request for profile
 */
static void get_profile(const std::string_view user_str, const fiy::Request& req) {
    // TODO if local request, check and see if the user already has a contact for that user

    const auto [user, dom] = fiy::host().split_user_str(user_str);

    // Handle local user
    if (dom.empty()) {
        // Check database
        const VC vc = DB::get_profile(
            std::string(dom.empty() ? user : user_str),
            req.user,
            req.domain);
        if (vc.invalid()) {
            req.respond(404, "", fiy::Body("Not found"));
            return;
        }

        // Respond with card
        if (req.find_header("Accept") == "application/json") {
            const auto body = vc.to_internal_json();
            req.respond(200, "Content-Type: application/json", fiy::Body(body));
        } else {
            const auto body = vc.to_vcard();
            req.respond(200, "Content-Type:text/vcard", fiy::Body(body));
        }
        return;
    }

    // Handle remote user
    const auto domain = std::string(dom);
    auto* remote_request = new fiy::Request(req);
    remote_request->domain = domain.c_str();
    remote_request->context = &req;
    remote_request->callback = [](const struct fiy::fiy_request_t* cb_req, const struct fiy::fiy_response_t* res) {
        auto* local_req = static_cast<const fiy::Request*>(cb_req->context);
        if (res == nullptr || res->status < 0) {
            const std::string body = concat(
                "Failed to get profile from ",
                std::string(cb_req->domain),
                ": ",
                res != nullptr
                    ? fiy::Body::to_string(res->body)
                    : "Peer error."
            );
            local_req->respond(500, "Content-Type: text/html", fiy::Body(body));
        } else {
            local_req->respond(*res);
        }
        delete cb_req;
    };

    fiy::host().request("contacts", remote_request);
}

/**
 * Profile picture endpoint
 * @param user_str relevant user string
 * @param req request
 * @param cb request callback
 */
static void get_pfp(const std::string_view user_str, const fiy::Request& req) {
    auto [user, dom] = fiy::host().split_user_str(user_str);

    static const fiy::fiy_response_t default_pfp {
        .status = 404,  // should this be 200 instead?
        .headers = "Content-Type: image/png\nCache-Control: max-age=300\nAccess-Control-Allow-Origin: *",
        .body = fiy::Body(
            (const char*)VC::default_pfp_raw,
            sizeof VC::default_pfp_raw
        )
    };

    // Local request
    const auto pfp_dataurl = DB::get_pfp(std::string(user),
        req.user,
        req.domain
    );
    // Either:
    // - dataurl: data:image/png;base64,data
    // - url: http(s) ...
    // - not found: "" empty string

    if (pfp_dataurl.empty()) {
        // Local user has no profile photo
        if (dom.empty()) {
            req.respond(default_pfp);
            return;
        }

        // Remote user, with no locally overridden profile picture
        //  -> forward request to remote server

        const auto domain = std::string(dom);
        auto* remote_request = new fiy::Request(req);
        remote_request->domain = domain.c_str();
        remote_request->context = static_cast<const void*>(&req);
        remote_request->callback = [](const struct fiy::fiy_request_t* cb_req, const struct fiy::fiy_response_t* res) {
            auto* local_req = static_cast<const fiy::Request*>(cb_req->context);
            if (!res || res->status < 0) {
                // Failed
                local_req->respond(default_pfp);
            } else {
                // Success, forward the response
                local_req->respond(*res);
            }
            delete cb_req;
        };
        fiy::host().request("contacts", remote_request);
        return;
    }

    // Redirect
    if (pfp_dataurl.starts_with("http")) {
        req.respond(307, "Location: " + pfp_dataurl, fiy::Body());
    }

    if (!pfp_dataurl.starts_with("data:")) {
        fiy::log_warning("PFP invalid PHOTO property, not a dataurl");
        req.respond(default_pfp);
        return;
    }
    std::string_view pfp = pfp_dataurl;
    pfp.remove_prefix(5);

    const auto end_type = pfp.find(';');
    const auto start_data = pfp.find(',');
    if (start_data == std::string::npos) {
        fiy::log_warning("PFP invalid PHOTO property");
        req.respond(default_pfp);
        return;
    }

    std::string_view media_type;
    if (end_type == std::string_view::npos) {
        fiy::log_warning("PFP invalid PHOTO property, dataurl missing media-type");
        media_type = "image";
    }
    media_type = pfp.substr(0, std::min(end_type, start_data));

    const auto data = pfp.substr(start_data + 1);
    const auto raw_data = base64::decode_into<std::string>(data);

    std::string headers = "Access-Control-Allow-Origin: *"
        "\nCache-Control: max-age=300\nContentType: ";
    headers += media_type;
    req.respond(200, headers, fiy::Body(raw_data));
}

static void handle_request(const struct fiy::fiy_request_t* request) {
    auto& req = *(const fiy::Request*) request;

    std::string_view path{req.path};

    // Get user profile
    if (req.method == (uint8_t) fiy::Request::Method::GET && path.starts_with("/profile/")) {
        path.remove_prefix(9);
        get_profile(path, req);
        return;
    }

    // Get user pfp
    if (path.starts_with("/pfp/")) {
        path.remove_prefix(5);
        get_pfp(path, req);
        return;
    }

    // Everything past here requires a login (either local or federated)
    static const fiy::Response no_auth_resp{
        303,
        "Location: " + fiy::host().host_base_uri() + "/portal/login",
        fiy::Body()
    };
    if (req.user == nullptr) {
        // For anon users, send them to login page
        if (req.domain == nullptr) {
            req.respond(no_auth_resp);
            return;
        }

        // For peer requests give 404 response
        req.respond(404);
        return;
    }

    // TODO accept shared contacts from other instances

    // Only local users beyond this point
    if (req.domain != nullptr) {
        req.respond(no_auth_resp);
        return;
    }

    if (path.starts_with("/main.css")) {
        req.respond(200,
            "Content-Type: text/css\nCache-Control: max-age=604800",
            Pages::main_css()
        );
        return;
    }
    if (path.starts_with("/main.js")) {
        req.respond(200,
            "Content-Type: text/javascript\nCache-Control: max-age=604800",
            Pages::main_js()
        );
        return;
    }

    // Fontawesome fonts
    if (path == "/fa/fa.css") {
        static constexpr char file_path[] = "font-awesome.css";
        req.respond(200,
            "Content-Type: text/css\nCache-Control: max-age=604800",
            Pages::mm_file_body<file_path>()
        );
        return;
    }
    if (path.starts_with("/fonts/fontawesome-webfont.")) {
        path.remove_prefix(27);
        if (path.starts_with("eot")) {
            static constexpr char file_path[] = "fontawesome-webfont.eot";
            req.respond(200,
                "Content-Type: application/vnd.ms-fontobject\nCache-Control: max-age=604800",
                Pages::mm_file_body<file_path>()
            );
            return;
        }
        if (path.starts_with("woff2")) {
            static constexpr char file_path[] = "fontawesome-webfont.woff2";
            req.respond(200,
                "Content-Type: font/woff2\nCache-Control: max-age=604800",
                Pages::mm_file_body<file_path>()
            );
            return;
        }
        if (path.starts_with("woff")) {
            static constexpr char file_path[] = "fontawesome-webfont.woff";
            req.respond(200,
                "Content-Type: font/woff\nCache-Control: max-age=604800",
                Pages::mm_file_body<file_path>()
            );
            return;
        }
        if (path.starts_with("ttf")) {
            static constexpr char file_path[] = "fontawesome-webfont.ttf";
            req.respond(200,
                "Content-Type: font/ttf\nCache-Control: max-age=604800",
                Pages::mm_file_body<file_path>()
            );
            return;
        }
        if (path.starts_with("svg")) {
            static constexpr char file_path[] = "fontawesome-webfont.svg";
            req.respond(200,
                "Cache-Control: max-age=604800",
                Pages::mm_file_body<file_path>()
            );
            return;
        }
    }

    // Icon
    if (path == "/icon.svg") {
        static constexpr char file_path[] = "icon.svg";
        req.respond(200,
            "Cache-Control: max-age=604800\nContent-Type: image/svg+xml",
            Pages::mm_file_body<file_path>()
        );
        return;
    }
    if (path == "/favicon.ico") {
        static constexpr char file_path[] = "favicon.ico";
        req.respond(200,
            "Cache-Control: max-age=604800\nContent-Type: image/x-icon",
            Pages::mm_file_body<file_path>()
        );
        return;
    }

    // Landing page
    if (path == "/" || (path.size() > 1 && path[1] == '?')) {
        req.respond(200, "Content-Type: text/html", Pages::index_html());
        return;
    }

    // Download all contacts
    if (path == "/all") {
        // User authenticated as we can see earlier
        if (req.domain != nullptr) {
            req.respond(401);
            return;
        }
        req.respond(200,
            "Content-Type: text/vcard",
            DB::get_user_rolodex(req.user)
        );
        return;
    }

    // TODO accept multiple cards in a single file
    if (path == "/save") {
        VC card;
        card.owner = req.user;
        card.parse(std::string(request->body, request->body_len));
        switch (DB::save_contact(card)) {
        case DB::Success:
            req.respond(200, "Content-Type: text/vcard", card.to_vcard());
            return;
        case DB::Error:
            req.respond(500, "Server Error");
            return;
        case DB::Unauthorized:
            req.respond(401, "Unauthorized");
            return;
        case DB::NotFound:
            req.respond(404, "Not Found");
            return;
        }
    }

    // Get card by id
    if (path.starts_with("/id/")) {
        path.remove_prefix(4);
        VC card;
        try {
            card.id = std::stoll(std::string(path));
        } catch (...) {
            req.respond(400, "", fiy::Body("Invalid contact ID"));
            return;
        }
        card.owner = req.user;

        if (DB::get_contact(card))
            req.respond(200, "Content-Type: text/vcard", card.to_vcard());
        else
            req.respond(404, "", fiy::Body("No card with given id"));
        return;
    }

    if (req.method == (uint8_t) fiy::Request::DELETE
        && path.starts_with("/delete/")
    ) {
        path.remove_prefix(7);
        uint64_t id;
        try {
            id = std::stoll(std::string(path));
        } catch (...) {
            req.respond(400, "", fiy::Body("Invalid contact ID"));
            return;
        }
        DB::delete_contact(req.user, id);

        // TODO
        req.respond(500, "", fiy::Body("TODO"));
        return;
    }

    if (path == "/tzdb") {
        // TODO 30 mins cache
        static const std::string tzdb_json = get_timezones_json();
        static const fiy::fiy_response_t tzdb_json_resp{
            .status = 200,
            .headers = "Content-Type: application/json\nCache-Control: max-age=604800",
            .body = fiy::Body(tzdb_json)
        };
        req.respond(tzdb_json_resp);
        return;
    }

    // Invalid path
    req.respond(404, "", fiy::Body("Not found"));
}

FIY_EXPORT fiy::ModInfo* start(const fiy::fiy_host_info_t* host_info) {
    static fiy::ModInfo mod_info = {
        .on_request = handle_request,
        .delete_user = DB::delete_user,
        .id="fiy.contacts",
        .version = "0.0"
    };
    fiy::host() = *host_info;
    return &mod_info;
}
