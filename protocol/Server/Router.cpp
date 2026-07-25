//
// Created by tate on 6/25/25.
//

// TODO shouldn't need this
#include <boost/url.hpp>

#include "../util/WebUtils.hpp"

#include "../FIY.hpp"

#include "Session.hpp"
#include "Router.hpp"
#include "Pages.hpp"
#include "DB.hpp"

// This file is getting kinda long, might make sense to break this up into separate files:
// - mod routes (mod request routing)
// - mod connector net api routes
// - portal routes (login, signup, portal, settings, etc.)

namespace http = boost::beast::http;

/**
 * Make mod request
 * @param conn connection to send response to
 * @param m mod to send request to (or nullptr for root mod)
 * @param uri path to request mod
 */
static void request_mod(std::shared_ptr<Session> conn, Mod* m, const std::string_view uri) {
    // No mod
    if (m == nullptr) {
        // No '' mod, use default index.html
        if (conn->req()["host"] == g_fiy->config.hostname
            && conn->req().target() == "/"
        ) {
            static const char subpath[] = "/index.html";
            Session::StringResponse res{
                http::status::ok,
                conn->req().version(),
                Pages::file_contents<subpath>()
            };
            res.set(http::field::content_type, "text/html");
            conn->respond(conn->prep(std::move(res)));
            DEBUG_LOG("No root mod, responding with landing page");
            return;
        }

        // Warn about missing mod
        Session::DynamicResponse res;
        res.result(404);
        boost::beast::ostream(res.body())
            << "<h1>404 - Could not find a mod to handle your request</h1>"
                "\n<hr/>\n<p>For a list of apps: check the <a href=\""
            << g_fiy->base_uri() << "/portal\">portal</a></p>\n";
        res.set(http::field::content_type, "text/html");
        conn->respond(conn->prep(std::move(res)));
        DEBUG_LOG("Invalid mod: " <<conn->req().target() <<" : " <<uri);
        return;
    }

    // Call mod
#ifdef FIY_DEBUG
    std::cout <<"Mod " <<m->id <<" : " << uri <<std::endl;
#endif
    conn->req().target(uri);
    m->ipc->handle_request(std::move(conn));
}

/**
 * Invoke app
 * @param conn connection
 */
static void mod_send_msg(std::shared_ptr<Session> conn) {
    Mod* mod;
    static Mod* root_mod = g_fiy->mods.get_mod("");

    // Mod-by-ID API
    auto path = conn->req().target();
    if (path.starts_with("/mods/")) {
        path.remove_prefix(6);
        const auto mod_end = path.find_first_of("/?");
        mod = g_fiy->mods.get_mod_by_id(
            path.substr(0, mod_end));
        std::string subpath = conn->req()["Fiy-Path"];
        if (subpath.empty() && mod_end != std::string_view::npos)
            subpath = path.substr(mod_end);
        if (subpath.empty())
            subpath = "/";
        request_mod(std::move(conn), mod, subpath);
        return;
    }

    // Subdomains
    const auto hostname = conn->req()[http::field::host];
    if (!hostname.empty()) {
        const std::string_view hhn = g_fiy->config.hostname;
        if (hhn != hostname) {
            // Subdomain mod  mod.example.com/uri/path
            if (hostname.ends_with(hhn)) {
                // Redirect to /<mod>/rest/of/path
                const auto mod_name = hostname.substr(0, hostname.size() - hhn.size() - 1);
                Session::EmptyResponse res{http::status::moved_permanently, conn->req().version()};
                const std::string new_path = concat('/', mod_name, path);
                res.set(http::field::location, new_path);
                conn->respond(std::move(res));
                return;
            }
            //            Session::DynamicResponse res;
            //            res.result(400);
            //            boost::beast::ostream(res.body())
            //                    << "Wrong host? Expected host to be "
            //                    << hhn.data()
            //                    << " but host was "
            //                    << hostname
            //                    << '\n';
            //            conn->respond(res);
            // Invalid host?
            LOG_ERR("Received request for invalid hostname: expected "
                << hhn.data()
                << " but host was "
                << hostname
                << '\n');
            request_mod(std::move(conn), nullptr, path);
            return;
        } // else: just a normal request
    }

    // NOTE: weird feature: Root Mods (ie - mod w/ path '')
    // this means that any time the mod path doesn't match
    // we need to assume it's a request for the root mod
    // so for example a request to /index.html would first check
    // for mod 'index.html' and if that doesn't exist it would try
    // mod '' with path '/index.html'

    // Not a subdomain mod
    const auto slash_idx = path.find('/', 1);
    if (slash_idx == std::string_view::npos) {
        // case: /
        // case: /mod
        const auto qs_idx = path.find('?', 1);
        if (qs_idx == std::string_view::npos) {
            // case: /
            if (path.size() <= 1) {
                request_mod(std::move(conn), root_mod, "/");
                return;
            }

            // case: /mod
            // case: /file_in_root_mod
            mod = g_fiy->mods.get_mod(path.substr(1));
            if (mod != nullptr) {
                request_mod(std::move(conn), mod, "/");
                return;
            }
            request_mod(std::move(conn), root_mod, path);
            return;
        }

        // case: /?param
        std::string sub_path = "/";
        sub_path += path.substr(qs_idx);
        if (qs_idx < 2) {
            request_mod(std::move(conn), root_mod, sub_path);
            return;
        }

        // case: /mod?param
        mod = g_fiy->mods.get_mod(path.substr(1, qs_idx - 1));
        if (mod != nullptr) {
            request_mod(std::move(conn), mod, sub_path);
            return;
        }

        // case: /file_in_root_mod?param
        request_mod(std::move(conn), root_mod, path);
        return;
    }

    // case: /mod/
    // case: /mod/uri/path
    // case: /mod/uri/path/
    // case: /mod/?param
    // case: /mod/uri/path?param
    // case: /mod/uri/path/?param
    // case: /path/in/root/mod
    mod = g_fiy->mods.get_mod(path.substr(1, slash_idx - 1));
    if (mod != nullptr) {
        request_mod(
            std::move(conn),
            g_fiy->mods.get_mod(path.substr(1, slash_idx - 1)),
            path.substr(slash_idx)
        );
        return;
    }
    request_mod(std::move(conn), root_mod, path);
}

// TODO ?redirect=/mod/that/requires/auth query parameter
static void signup_post(std::shared_ptr<Session>&& conn) {
    // Cache prepared responses
    static const auto resp_bad_username
        = Pages::signup_page(400, "Username should contain only alphanumeric characters");
    static const auto resp_username_taken
        = Pages::signup_page(400, "Username is taken, try a different one");
    static const auto resp_username_reserved
        = Pages::signup_page(400, "Username is reserved, try a different one");
    static const auto resp_bad_form
        = Pages::signup_page(400, "Form Error");
    static const auto resp_long_username
        = Pages::signup_page(400, "Username must be less than " + std::to_string(LocalUser::USERNAME_MAX_LENGTH) +  " characters");
    static const auto resp_long_name
        = Pages::signup_page(400, "Name must be less than " + std::to_string(LocalUser::NAME_MAX_LENGTH) +  " characters");
    static const auto resp_long_contact
        = Pages::signup_page(400, "Contact info must be less than " + std::to_string(LocalUser::EMAIL_MAX_LENGTH) + " characters");
    static const auto resp_server_error
        = Pages::signup_page(500, "Failed to create account, try again later");

    // Parse form body
    std::deque<std::pair<std::string, std::string>> form;
    if (!WebUtils::parse_form_url_encoded(conn->req().body(), form)) {
        conn->respond(conn->prep(resp_bad_form));
        DEBUG_LOG("invalid body: '" <<conn->req().body() <<"'");
        return;
    }
    std::string username, password, contact, name;
    for (auto&& [k, v] : std::move(form)) {
        if (k == "username")
            username = std::move(v);
        else if (k == "password")
            password = std::move(v);
        else if (k == "contact")
            contact = std::move(v);
        else if (k == "name")
            name = std::move(v);
        else
            DEBUG_LOG("invalid form field: " <<k);
    }
    LOG("Creating local user: " << username <<"  | " << contact);

    // Validate username
    for (const auto& c : username)
        if (! (isalnum(c) || c == '_')) {
            conn->respond(conn->prep(resp_bad_username));
            return;
        }
    if (username.size() > LocalUser::USERNAME_MAX_LENGTH || username.empty()) {
        conn->respond(conn->prep(resp_long_username));
        return;
    }
    static const std::unordered_set<std::string_view> reserved_usernames = {
        "api", "mod", "app", "help", "home", "index", "new", "about",
        "error", "create", "delete", "post", "view", "search", "notifications"
    };
    if (username.size() == 1
        || reserved_usernames.contains(username)
        || username.ends_with("settings")
    ) {
        conn->respond(conn->prep(resp_username_reserved));
        return;
    }
    if (g_fiy->users.get_user(username) != nullptr) {
        conn->respond(conn->prep(resp_username_taken));
        return;
    }

    // Validate name
    if (name.size() > LocalUser::NAME_MAX_LENGTH) {
        conn->respond(conn->prep(resp_long_name));
        return;
    }

    // Validate contact
    if (contact.size() > LocalUser::EMAIL_MAX_LENGTH) {
        conn->respond(conn->prep(resp_long_contact));
        return;
    }

    // Create user
    LocalUser user{username, false, contact, g_fiy->now()};
    try {
        if (!g_fiy->users.add_user(user, password)) {
            LOG_ERR("Failed to create user?");
            conn->respond(conn->prep(resp_server_error));
            return;
        }
    } catch (const DB::Exception& e) {
        LOG_ERR("Failed to create new user: Database Error: " <<e.what());
        conn->respond(conn->prep(resp_server_error));
        return;
    }

    auto auth_token = g_fiy->users.login_user(username, password);

    // Find redirect query parameter
    Session::EmptyResponse res;
    res.result(http::status::see_other); // 303: post -> get
    // FIXME? https://stackoverflow.com/questions/18492576/share-cookies-between-subdomain-and-domain
    conn->clear_cookie_cache();
    res.set(http::field::set_cookie,
        WebUtils::serialize_cookie(
            "fiy_auth",
            auth_token.m_token,
            {
                .encode_fn=nullptr,
                .max_age=LocalUser::AuthToken::SESSION_LIFETIME,
                .path="/",
                .http_only=true
            }
        )
    );

    const auto ps = boost::urls::url_view(conn->req().target()).params();
    if (auto it = ps.find("redirect"); it != ps.end())
        res.set(http::field::location, (*it).value);
    else
        res.set(http::field::location, "/portal");
    conn->respond(conn->prep(std::move(res)));
}

static void login_post(std::shared_ptr<Session>&& conn) {
    // Pre-allocated responses
    static const auto resp_bad_form = Pages::login_page(400, "Form Error");
    static const auto resp_bad_creds = Pages::login_page(401, "Incorrect username/password");

    // Get username and password from body
    std::deque<std::pair<std::string, std::string>> form;
    if (!WebUtils::parse_form_url_encoded(conn->req().body(), form)) {
        conn->respond(conn->prep(resp_bad_form));
        DEBUG_LOG("invalid body: '" <<conn->req().body() <<"'");
        return;
    }
    std::string username, password;
    for (auto&& [k, v]: std::move(form)) {
        if (k == "username")
            username = std::move(v);
        else if (k == "password")
            password = std::move(v);
        else {
            DEBUG_LOG("invalid form field: " << k );
        }
    }

    // root user?
//    if (username.empty()) {
//        callback(m_response_invalid_body);
//        DEBUG_LOG("empty username");
//        return;
//    }
//    if (password.empty()) {
//        callback(resp_bad_form);
//        DEBUG_LOG("empty password");
//        return;
//    }

    // Get auth token for user
    const auto auth_token = g_fiy->users.login_user(username, std::move(password));
    if (auth_token.m_user == nullptr) {
        conn->respond(conn->prep(resp_bad_creds));
        DEBUG_LOG("wrong username/password for user " <<username);
        return;
    }

    // Log user in and send them to portal home
    Session::EmptyResponse res;
    res.result(http::status::see_other); // 303 - redirect them with get request
    // FIXME? https://stackoverflow.com/questions/18492576/share-cookies-between-subdomain-and-domain
    conn->clear_cookie_cache();
    res.set(http::field::set_cookie,
        WebUtils::serialize_cookie(
            "fiy_auth",
            auth_token.m_token,
            {
                .encode_fn=nullptr,
                .max_age=LocalUser::AuthToken::SESSION_LIFETIME,
                .path="/",
                .http_only=true
            }
        )
    );
    const auto ps = boost::urls::url_view(conn->req().target()).params();
    if (auto it = ps.find("redirect"); it != ps.end())
        res.set(http::field::location, (*it).value);
    else
        res.set(http::field::location, "/portal");
    conn->respond(conn->prep(std::move(res)));
}

/// Sent to us from a peer that wants to connect
static void peer_handshake(std::shared_ptr<Session>&& conn) {
    // TODO move logic to Peers::handshake ?
    switch (g_fiy->config.federation) {
        case FiyConfig::FederationStatus::ENABLED:
            break;
        case FiyConfig::FederationStatus::DISABLED:
        case FiyConfig::FederationStatus::REJECT_INCOMING: {
            Session::StringResponse res;
            res.result(http::status::unauthorized);
            res.body() = "This instance is not accepting federation requests right now.";
            conn->respond(conn->prep(std::move(res)));
            return;
        }
    }

    std::string signature;
    auto peer = Peer::parse_handshake_request(conn->req().body(), signature);
    if (peer == nullptr) {
        DEBUG_LOG("Invalid handshake body");
        Session::StringResponse res;
        res.result(400);
        res.body() = "Invalid handshake body";
        conn->respond(conn->prep(std::move(res)));
        return;
    }

    // TODO is there a reason/process for re-authentication
    if (g_fiy->peers.get_peer_for_domain(peer->domain) != nullptr) {
        Session::StringResponse res;
        res.result(400);
        res.body() = "Already peers";
        conn->respond(conn->prep(std::move(res)));
        return;
    }

    // Get foreign public key
    auto with_pubkey_cb = [
        peer, conn, handshake_sig = std::move(signature)
    ] (http::response<http::string_body> res) {
        // Make sure it's success
        if (res.result_int() != 200) {
            Session::StringResponse ret;
            ret.result(ret.result());
            ret.body() = "Failed to get public key from domain " + std::string(peer->domain);
            conn->respond(conn->prep(std::move(ret)));
            LOG_ERR("Failed to get public key for handshake peer "
                <<peer->domain <<": " <<ret.result_int());
        }

        // Verify signature
        const auto pubkey = res.body();

        const auto sig_data = concat(
            peer->domain,
            '\n',
            peer->auth.sym_key,
            '\n',
            peer->auth.bearer_token_we_send
        );
        if (!Crypto::SSL::verify(pubkey, sig_data, handshake_sig)) {
            Session::StringResponse ret;
            ret.result(401);
            ret.body() = "Invalid signature";
            conn->respond(conn->prep(std::move(ret)));
            DEBUG_LOG("Invalid signature received in handshake");
            return;
        }

        // Our contribution to the secret
        std::string our_secret = PeerAuth::get_token_string();
        peer->auth.sym_key += our_secret;

        int max_tries = 10;
        while (!g_fiy->peers.add_peer(peer->domain, peer) && max_tries-- > 0) {
            // Maybe another thread already completed the handshake?
            if (g_fiy->peers.get_peer_for_domain(peer->domain) != nullptr) {
                Session::StringResponse ret;
                ret.result(400);
                ret.body() = "Already peers";
                conn->respond(conn->prep(std::move(ret)));
                return;
            }

            // Update token and try again
            peer->auth.bearer_token_we_accept = PeerAuth::get_token_string();
        }

        // Respond with
        //  - bearer token
        //  - our part of the secret
        //  - signature
        std::string response = concat(
            peer->auth.bearer_token_we_accept,
            '\n',
            our_secret
        );
        auto sig = Crypto::SSL::sign(g_fiy->config.private_key, response);
        response += '\n' + sig;

        Session::StringResponse ret;
        ret.result(200);
        // TODO only encrypt the secret and bearer token
        // ret.body() = Crypto::SSL::encrypt(pubkey, response);
        ret.body() = response;
        conn->respond(conn->prep(std::move(ret)));
    };

    auto err_key_cb = [peer, conn](const std::string& message) {
        Session::StringResponse res;
        res.result(404);
        res.body() = "Could not get public key from domain " + std::string(peer->domain);
        conn->respond(conn->prep(std::move(res)));
        LOG_ERR("Could not get public key for handshake peer " <<peer->domain <<": " <<message);
    };

    http::request<http::empty_body> key_req;
    key_req.target("/peer/key");
    key_req.method(http::verb::get);
    // key_req.keep_alive(false);

    if (strchr(peer->domain, ':') == nullptr) {
        g_fiy->https.request(
            peer->domain,
            std::move(key_req),
            std::move(with_pubkey_cb),
            std::move(err_key_cb)
        );
    } else {
        g_fiy->http.request(
            peer->domain,
            std::move(key_req),
            std::move(with_pubkey_cb),
            std::move(err_key_cb)
        );
    }
}

/// API used by ModConnectorNet
static void login_user_internal(const std::shared_ptr<Session>& conn) {
    // TODO this should probably be encrypted or something
    const auto token = conn->req()["fiy-auth"];
    const auto username = conn->req()["fiy-user"];
    const auto password = conn->req()["fiy-pass"];

    if (! g_fiy->mods.has_net_connector(token)) {
        http::response<http::empty_body> res;
        res.result(401);
        conn->respond(conn->prep(std::move(res)));
        return;
    }

    // TODO maybe we should sign response?
    const auto r = LocalUsers::auth_user(username, std::move(password));
    http::response<http::empty_body> res;
    res.result(r == 0 ? 200 : r == 1 ? 401 : 500);
    conn->respond(conn->prep(std::move(res)));
}

void route_request(std::shared_ptr<Session> conn) {
    auto path = conn->req().target();

    // Logging
#ifdef FIY_DEBUG
    std::cout <<"Request: "
        <<conn->address().to_string()
        <<":"
        <<conn->port()
        <<" -- "
        <<http::to_string(conn->req().method())
        <<" "
        <<path
        <<std::endl;
#endif

    switch (conn->req().method()) {
        case http::verb::get:
            if (path.starts_with("/portal")) {
                path.remove_prefix(7);
                if (path.starts_with("/login")) {
                    conn->respond(conn->prep(Pages::login_page()));
                    return;
                } else if (path.starts_with("/signup")) {
                    conn->respond(conn->prep(Pages::signup_page()));
                    return;
                } else if (path.starts_with("/theme.js")) {
                    // Send cached file contents
                    static constexpr char subpath[] = "/theme.js";
                    Session::StringResponse res{
                        http::status::ok,
                        conn->req().version(),
                        Pages::mm_file<subpath>()
                    };
                    res.set(http::field::content_type, "text/javascript");
                    conn->respond(conn->prep(std::move(res)));
                    return;
                } else if (path.starts_with("/main.css")) {
                    // Send cached file contents
                    static constexpr char subpath[] = "/assets/minstyle.io.min.css";
                    Session::StringResponse res{
                        http::status::ok,
                        conn->req().version(),
                        Pages::mm_file<subpath>()
                    };
                    res.set(http::field::content_type, "text/css");
                    res.set(http::field::cache_control, "max-age=604800");
                    conn->respond(conn->prep(std::move(res)));
                    return;
                } else if (path.starts_with("/app.svg")) {
                    static constexpr char subpath[] = "/assets/app.svg";
                    Session::StringResponse res{
                        http::status::ok,
                        conn->req().version(),
                        Pages::mm_file<subpath>()
                    };
                    res.set(http::field::content_type, "image/svg+xml");
                    res.set(http::field::cache_control, "max-age=604800");
                    conn->respond(conn->prep(std::move(res)));
                    return;
                } else if (path.starts_with("/redirect")) {
                    // Check for redirect header
                    if (auto it = conn->req().find("FIY-Redirect");
                        it != conn->req().end()
                    ) {
                        Session::EmptyResponse res;
                        res.result(http::status::see_other); // 303 -> redirect as GET
                        res.set(http::field::location, it->value());
                        conn->respond(conn->prep(std::move(res)));
                        return;
                    }

                    // No redirect header, check for query params
                    const auto ps = boost::urls::url_view(conn->req().target()).params();
                    if (auto it = ps.find("redirect"); it != ps.end()) {
                        Session::EmptyResponse res;
                        res.result(http::status::see_other);
                        res.set(http::field::location, (*it).value);
                        conn->respond(conn->prep(std::move(res)));
                        return;
                    }

                    // Invalid request
                    Session::StringResponse res;
                    res.result(http::status::bad_request);
                    res.body() = "Invalid redirect: no destination provided";
                    conn->respond(conn->prep(std::move(res)));
                    return;
                } else if (path.empty() || path == "/") {
                    // Authenticate user
                    auto user = conn->find_user_local();
                    if (user == nullptr) {
                        DEBUG_LOG("User not logged in");
                        Session::EmptyResponse res;
                        res.result(http::status::see_other);
                        res.set(http::field::location, "/portal/login");
                        conn->respond(conn->prep(std::move(res)));
                        return;
                    }

                    // Send them to portal home
                    conn->respond(conn->prep(Pages::portal_apps(*user)));
                    return;
                } else {
                    std::cerr <<"404 -- GET /portal : " <<path <<std::endl;
                    Session::StringResponse res;
                    res.body() = "404 not found!\n";
                    res.result(404);
                    conn->respond(conn->prep(std::move(res)));
                    DEBUG_LOG("404 - " <<conn->req().target());
                    return;
                }
            } else if (path == "/peer/key") {
                if (g_fiy->config.federation == FiyConfig::FederationStatus::DISABLED) {
                    Session::EmptyResponse res;
                    res.result(http::status::unauthorized);
                    conn->respond(conn->prep(std::move(res)));
                    return;
                }

                // Send key
                Session::StringResponse res;
                res.result(200);
                res.set("Fiy-Now", std::to_string(g_fiy->now()));
                res.body() = g_fiy->config.public_key;
                conn->respond(conn->prep(std::move(res)));
                return;
            } else if (path == "/robots.txt") {
                Session::StringResponse res;
                res.result(200);
                static constexpr char robots_txt[] = "/robots.txt";
                res.body() = Pages::file_contents<robots_txt>();
                res.set(http::field::content_type, "text/plain");
                res.set("Cache-control", "max-age=60000");
                conn->respond(conn->prep(std::move(res)));
                return;
            } else {
                mod_send_msg(std::move(conn));
                return;
            }

        case http::verb::post:
            if (path.starts_with("/portal")) {
                path.remove_prefix(7);
                if (path.starts_with("/login")) {
                    // TODO ?redirect=/mod/that/requires/auth query parameter
                    login_post(std::move(conn));
                    return;
                } else if (path.starts_with("/signup")) {
                    // TODO ?redirect=/mod/that/requires/auth query parameter
                    signup_post(std::move(conn));
                    return;
                } else {
                    std::cerr <<"404 -- POST /portal : " <<path <<std::endl;
                    Session::StringResponse res;
                    res.body() = "404 not found!\n";
                    res.result(404);
                    conn->respond(conn->prep(std::move(res)));
                    return;
                }
            } else if (path == "/peer/handshake") {
                peer_handshake(std::move(conn));
                return;
            } else {
                mod_send_msg(std::move(conn));
                return;
            }

        default:
            mod_send_msg(std::move(conn));
            return;
    }
}