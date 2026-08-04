//
// Created by tate on 4/10/26.
//

#include "ModConnectorNet.hpp"



#include <ranges>
#include <functional>

#include <dlfcn.h>

#include <nlohmann/json.hpp>


#include "../Server/Session.hpp"
#include "../FIY.hpp"

bool ModConnectorNet::start() {
    // JSON payload containing fields: bearer token, mod id, base_uri, now
    do {
        m_bearer_accept = PeerAuth::get_token_string();
    } while (!g_fiy->mods.add_net_connector(m_bearer_accept, this));
    const std::string payload = "{\"bearer\":\"" + m_bearer_accept
        + "\",\"id\":\""+ m_mod->id
        + "\",\"base_uri\":\""
        + g_fiy->config.protocol
        + g_fiy->config.hostname
        + '/' + m_mod->path
        + "\",\"now\":" + std::to_string(g_fiy->now()) + "}";

    // Generate request
    using namespace boost::beast;
    http::request<http::string_body> req;
    req.method(http::verb::post);
    req.target("/start");
    req.body() = payload;
    req.set("Fiy-Signature", Crypto::SSL::sign(g_fiy->config.private_key, payload));
    req.keep_alive(false);
    req.prepare_payload();

    // Callback handler
    auto cb = [this](http::response<http::string_body> res) {
        std::string s = res.body();
        auto j = nlohmann::json::parse(s);
        if (!j.is_object()) {
            m_mod->error("bad response, should be an object");
            return;
        }
        if (!j.contains("accept") && j["accept"].is_string()) {
            m_mod->error("bad response, missing 'accept' field ");
            return;
        } else {
            m_bearer_send = j["accept"].get<std::string>();
        }
        if (j.contains("id")
            && m_mod->id.empty()
            && j["id"].is_string()
        ) {
            m_mod->id = j["id"].get<std::string>();
        }
        if (j.contains("version")
            // && !m_mod->m_version.initialized()
            && j["version"].is_string()
        ) {
            m_mod->version = Version(j["version"].get<std::string>());
        }
    };
    auto err_cb = [this](const std::string& err) {
        std::cerr << "Mod " << m_mod->id <<": Failed to connect to " <<m_uri <<std::endl;
        m_mod->error(err);
    };

    // Make request
    if (m_https)
        g_fiy->https.request(m_uri, req, cb, err_cb);
    else
        g_fiy->http.request(m_uri, req, cb, err_cb);

    // hostinfo should also include authentication system
    // ie - credentials we can validate for every request
    return true;
}

bool ModConnectorNet::stop() {
    // Already stopped
    if (m_bearer_accept.empty()) {
        DEBUG_LOG("Already stopped");
        return true;
    }

    g_fiy->mods.remove_net_connector(m_bearer_accept);
    m_bearer_accept.clear();

    using namespace boost::beast;
    http::request<http::empty_body> req;
    req.method(http::verb::post);
    req.target("/stop");
    req.set("Fiy-Auth", m_bearer_send);
    req.keep_alive(false);

    auto cb = [this](auto resp) {
        if (resp.result() != http::status::ok)
            DEBUG_LOG("Mod " <<m_mod->id <<": failed to stop: HTTP " <<resp.result());
        else
            DEBUG_LOG("Mod " <<m_mod->id <<": stopped");
    };
    auto err_cb = [this](auto err) {
        DEBUG_LOG("Mod " <<m_mod->id <<": failed to stop: " <<err);
        (void)err;
    };

    if (m_https)
        g_fiy->https.request(m_uri, req, cb, err_cb);
    else
        g_fiy->http.request(m_uri, req, cb, err_cb);
    return true;
}

void ModConnectorNet::handle_request(std::shared_ptr<Session> conn) {
    // TODO probably shouldn't send the fiy_auth cookie
    //  but mods should be trusted so not concerning

    const auto user = conn->find_user();
    if (user == std::nullopt)
        return;
    conn->req().set("Fiy-User", user->str());
    conn->req().set("Fiy-Auth", m_bearer_send);
    std::string target = "/request";
    target += conn->req().target();
    conn->req().target() = target;

    auto cb = [session = std::move(conn)] (auto resp) {
        session->respond(std::move(resp));
    };
    auto err_cb = [this, session = std::move(conn)] (std::string err) {
        Session::StringResponse res;
        res.result(500);
        res.body() = "<h1>Server Error</h1><p>Request failed: " + err + "</p>";
        res.set(boost::beast::http::field::content_type, "text/html");
        session->respond(session->prep(std::move(res)));
        LOG_ERR("ModNetConnector[" <<m_mod->id <<"]: ");
    };

    if (m_https)
        g_fiy->https.request(m_uri, conn->req(), cb, err_cb);
    else
        g_fiy->http.request(m_uri, conn->req(), cb, err_cb);
}

void ModConnectorNet::delete_user(const char* user) {
    boost::beast::http::request<boost::beast::http::empty_body> req;
    req.target("/user");
    req.method(boost::beast::http::verb::delete_);
    req.set("Fiy-Auth", m_bearer_send);
    req.set("Fiy-User", user);
    req.keep_alive(false);

    auto cb = [this] (auto resp) {
        if (resp.result_int() != 200)
            LOG("Mod " <<m_mod->id <<": failed to delete user: HTTP code " <<resp.result());
    };
    auto err_cb = [this, user] (auto err) {
        LOG("Mod " <<m_mod->id <<": failed to delete user: " <<user <<" -- " <<err);
    };

    if (m_https)
        g_fiy->https.request(m_uri, req, cb, err_cb);
    else
        g_fiy->http.request(m_uri, req, cb, err_cb);
}

void ModConnectorNet::handle_request(const fiy::fiy_request_t* req) {
    const fiy::Request& r = *req;
    namespace http = boost::beast::http;
    http::request<http::string_body> net_req;
    net_req.method(http::verb::post);
    net_req.target() = std::string("/request") + r.path;
    net_req.set("Fiy-Auth", m_bearer_send);
    net_req.set("Fiy-User", r.user);
    net_req.set("Fiy-Domain", r.domain);
    net_req.set("Fiy-Method",
        http::to_string(static_cast<http::verb>(r.method)));
    for ( const auto& [k, v] : r.headers_map())
        net_req.set(k, v);
    net_req.body() = r.body;
    net_req.keep_alive(false);
    net_req.prepare_payload();

    auto cb = [req] (auto resp) {
        if (req->callback == nullptr)
            return;

        // Convert headers into a string
        std::string headers_str;
        for (const auto& f : resp.base()) {
            headers_str += f.name_string();
            headers_str += ": ";
            headers_str += f.value();
            headers_str += '\n';
        }

        const fiy::fiy_response_t response{
            .status = static_cast<int>(resp.result_int()),
            .headers = headers_str.c_str(),
            .body = fiy::Body(resp.body())
        };
        req->callback(req, &response);
    };
    auto err_cb = [this, req] (auto err) {
        DEBUG_LOG("Mod " <<m_mod->id <<": handle request failed " <<err);
        if (req->callback == nullptr)
            return;

        const fiy::fiy_response_t response{
            .status = -1,
            .headers = "",
            .body = fiy::Body(err),
        };
        req->callback(req, &response);
    };

    if (m_https)
        g_fiy->https.request(m_uri, net_req, cb, err_cb);
    else
        g_fiy->http.request(m_uri, net_req, cb, err_cb);
}
