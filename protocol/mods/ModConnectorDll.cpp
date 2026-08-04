//
// Created by tate on 4/10/26.
//

#include "ModConnectorDll.hpp"

#include <dlfcn.h>

#include <nlohmann/json.hpp>

#include "../../util/ThreadPool.hpp"
#include "../../util/FileCache.hpp"

#include "../../modlib/fiymod.hpp"

#include "../FIY.hpp"
#include "../DB.hpp"

#include "../Server/util.hpp"

#include "../users/LocalUser.hpp"
#include "../Server/Session.hpp"


struct ModConnectorDll::ModDLLHostInfo : fiy::fiy_host_info_t {
private:
    // TODO these should probably be unique_ptr's ?
    std::string m_base_uri;
public:

    explicit ModDLLHostInfo(const Mod* mod) {
        this->log = [](const int n, const char* s){
            static const char* types[] = { "FATAL", "ERROR", "WARN", "INFO", "DEBUG" };
            const char* type_str;
            if (n > 4 || n < 0) {
                LOG_WARN("Mod used fiy_host_info.log with invalid log type");
                type_str = "INVALID";
            } else {
                type_str = types[n];
            }

            std::cerr <<"Mod: " <<type_str <<": " <<s <<std::endl;
        };
        this->now = []() { return g_fiy->now(); };

        this->m_base_uri = concat(
            g_fiy->config.protocol,
            g_fiy->config.hostname,
            '/',
            mod->path
        );
        this->base_uri = m_base_uri.c_str(); // host info shouldn't be moved either
        this->request = ModDLLHostInfo::request_impl;
        this->domain = g_fiy->config.hostname;
        this->app_id = mod->id.c_str(); // safe assuming the mod isn't moved
        this->local_login = ModDLLHostInfo::local_login_impl;
        this->user_info = ModDLLHostInfo::user_info_impl;
        this->data_dir = mod->user_data_dir.c_str();
        this->mod_config = mod->config.c_str();
    }

    /**
     * Send a request to another app
     * @param app_id app to send the request to
     * @param request request to send to the other app
     *      method   - http method
     *      path     - uri path
     *      domain   - remote server to send request to or nullptr if local inter-app request
     *      user     - local user or nullptr if unauthenticated
     * @param context this pointer is passed back to the callback at the end
     * @param callback called with response
     * @notes
     * - local apps can send requests to each other without restrictions
     * - an app on server a can only send requests to apps on server b on behalf of users residing on server a
     *    - this prevents false impersonation
     */
    static void request_impl(
        const char* app_id,
        const fiy::fiy_request_t* request
    ) {
        // Send request to peer
        g_fiy->peers.request_peer(request->domain, app_id, request);
    }

    /**
     * Authenticate an instance-local user
     * @param username user's username
     * @param password user's password
     * @return
     *  0 on success
     *  1 if the user does not exist
     *  -1 error
     */
    static int local_login_impl(const char* username, const char* password) {
        return LocalUsers::auth_user(username, password);
    }

    /**
     * Get information for a given local user
     * @param local_user_name username for relevant user
     * @param ret struct to put the results into, or null to check existence
     * @return
     *  0 on success
     *  1 if the user does not exist
     *  -1 error
     */
    static int user_info_impl(const char* local_user_name, fiy::fiy_local_user_info_t* ret) {
        try {
            const auto u = g_fiy->users.get_user(local_user_name);
            if (u == nullptr)
                return 1;
            if (ret == nullptr)
                return 0;

            ret->admin = u->is_admin;
            ret->join_ts = u->joined_ts;
            return 0;
        } catch (const DB::Exception& e) {
            LOG_ERR("Database Error: " <<e.what());
#ifdef FIY_DEBUG
            throw e;
#endif
            return -1;
        }
    }
};

/// Message passed to shared library
class ModDllConnectorRequest : public fiy::fiy_request_t {
    std::shared_ptr<Session> m_conn;
    std::string m_str_path;
    std::string m_str_user;
    std::string m_str_headers;

public:

    using RequestHandler = void (*)(const fiy_request_t*);
    RequestHandler on_request;

    // explicit ModDllConnectorRequest(std::shared_ptr<Session> conn, RequestHandler on_request):
    //     ModDllConnectorRequest(std::move(conn), conn->find_user(), on_request)
    // {}

    explicit ModDllConnectorRequest(
        std::shared_ptr<Session> conn,
        const Session::User& user,
        const RequestHandler handle_request
    ):
        m_conn(std::move(conn)),
        on_request(handle_request)
    {
        auto& r = m_conn->req();

        // Initialize fiy_request_t
        this->method = static_cast<uint8_t>(r.method());

        // TODO maybe change API to ptr+len to avoid this copy
        this->m_str_path = r.target();
        this->path = this->m_str_path.c_str();

        this->body = r.body().data(); // this should be safe bc we have shared_ptr reference
        this->body_len = r.body().size();

        this->domain = user.domain;

        if (user.user.empty() || user.user == " ") {
            this->user = nullptr;
        } else {
            this->m_str_user = user.user;
            this->user = this->m_str_user.c_str();
        }

        set_this_headers(m_conn->req().base());

        this->context = static_cast<const void*>(this);
        this->callback = [](const fiy::fiy_request_t* req, const fiy_response_t* res) {
            auto* self = static_cast<const ModDllConnectorRequest*>(req->context);
            self->respond(res);
        };
    }

    [[nodiscard]] bool request_is_local() const {
        return domain == nullptr && user != nullptr;
    }

    void respond(const fiy::fiy_response_t* r) const {
        switch (r->body.type) {
            case fiy::BodyType::FIY_BODY_NONE: {
                Session::EmptyResponse res;
                res.result(r->status);
                response_set_headers(res, r->headers, request_is_local() );
                m_conn->respond(m_conn->prep(std::move(res)));
                break;
            }

            case fiy::BodyType::FIY_BODY_FILE: {
                // Convert to boost file type
                boost::beast::file_posix fp;
                fp.native_handle(r->body.file.fd);
                boost::beast::error_code ec;
                if (r->body.file.offset != 0) {
                    fp.seek(r->body.file.offset, ec);
                    if (ec) {
                        LOG_ERR("Seek failed: " << ec.message());
                        break;
                    }
                }

                // Construct response
                Session::FileResponse res;
                res.body().reset(std::move(fp), ec); // note: have to use reset or else it will set size = 0
                if (ec) {
                    LOG_ERR("Failed to set file: " << ec.message());
                    Session::StringResponse resp;
                    resp.body() = "Server Error: Failed to set body";
                    resp.result(500);
                    m_conn->respond(m_conn->prep(std::move(resp)));
                    break;
                }
                res.result(r->status);
                response_set_headers(res, r->headers, request_is_local());
                m_conn->respond(m_conn->prep(std::move(res)));
                break;
            }

            case fiy::BodyType::FIY_BODY_BUFFER:
            case fiy::BodyType::FIY_BODY_READER: {
                Session::StringResponse res;
                res.body() = fiy::Body::to_string(r->body);
                res.result(r->status);
                response_set_headers(res, r->headers, request_is_local());
                m_conn->respond(m_conn->prep(std::move(res)));
                break;
            }
        }
        delete this; // TODO Pool
    }

    // static char* new_cstr_from_string(const std::string_view s) {
    //     const auto l = s.size();
    //     char* ret = new char[l + 1];
    //     if (!s.empty())
    //         memcpy(ret, s.data(), l);
    //     ret[l] = '\0';
    //     return ret;
    // }

    void set_this_headers(const boost::beast::http::fields& f) {
        if (f.cbegin() == f.cend()) {
            this->headers = nullptr;
            return;
        }
        m_str_headers = "";
        auto it = f.cbegin();
        m_str_headers += it->name_string();
        m_str_headers += ": ";
        m_str_headers += it->value();
        for (; it != f.cend(); ++it) {
            m_str_headers += "\r\n";
            m_str_headers += it->name_string();
            m_str_headers += ": ";
            m_str_headers += it->value();
        }
        this->headers = m_str_headers.c_str();
    }
};

bool ModConnectorDll::stop() {
    // Nothing to stop
    if (m_dl_handle == nullptr)
        return true;

    // Call stop function
    const auto stop_fn = (void(*)()) dlsym(m_dl_handle, "start");
    if (stop_fn != nullptr)
        stop_fn();

    // Close handle
    auto ret = dlclose(m_dl_handle);
    if (ret == 0) {
        m_dl_handle = nullptr;
    } else {
        LOG_ERR("dlclose() gave" << ret <<": " <<dlerror());
    }

    // Cleanup
    delete m_host_info;
    return ret == 0;
}

bool ModConnectorDll::start() {
    // Generate host info
    delete m_host_info;
    m_host_info = new ModDLLHostInfo(m_mod);

    // Open shared object
    if (m_dl_handle != nullptr) {
        DEBUG_LOG("Handle replaced!");
    }
    m_dl_handle = dlopen(m_uri.c_str(), RTLD_LAZY | RTLD_LOCAL);
    if (m_dl_handle == nullptr) {
        DEBUG_LOG(this->m_mod->name <<": dlopen failed: " <<dlerror());
        return false;
    }

    // Start mod
    const auto start_fn = (fiy::StartFunction) dlsym(m_dl_handle, "start");
    if (start_fn == nullptr) {
        DEBUG_LOG(this->m_mod->name <<": could not find start function -- " <<dlerror());
        return false;
    }
    m_mod_info = start_fn(m_host_info);
    if (m_mod_info == nullptr) {
        DEBUG_LOG(this->m_mod->name <<": start() returned null");
        return false;
    }

    // Update fields
    if (m_mod_info->version != nullptr /* && !m_mod->m_version.initialized() */)
        m_mod->version = Version(m_mod_info->version);
    if (m_mod_info->id != nullptr && m_mod->id.empty())
        m_mod->id = m_mod_info->id;
    return true;
}

void ModConnectorDll::delete_user(const char* user) {
    if (m_mod_info == nullptr || m_mod_info->delete_user == nullptr)
        return;
    m_mod_info->delete_user(user);
}

void ModConnectorDll::handle_request(std::shared_ptr<Session> conn) {
    // Authenticate user
    const auto user_check = conn->find_user();
    if (!user_check)
        return;
    const auto& user = *user_check;

    // Check access restrictions
    const char* username = user.user.empty() ? nullptr : user.user.c_str();
    if (m_mod->can_access == nullptr
        || !m_mod->can_access(username, user.domain)
    ) {
        static const auto body_str = "<h1>401 - Unauthorized</h1><hr/><a href='"
            + g_fiy->base_uri() + "/portal'>Go to portal</a>";
        Session::StringResponse res;
        res.result(401);
        res.body() = body_str;
        conn->respond(conn->prep(res));
        return;
    }

    // Safety check
    if (m_mod_info == nullptr || m_mod_info->on_request == nullptr) {
        boost::beast::http::response<boost::beast::http::string_body> res;
        res.result(500);
        res.body() = "Module not initialized";
        conn->respond(conn->prep(std::move(res)));
        DEBUG_LOG("Mod " <<m_mod->id <<": called before start()");
        return;
    }

    // Call mods in separate threadpool so that they don't block asio threads
    static ThreadPool<ModDllConnectorRequest*> request_handler{
        [](const ModDllConnectorRequest* r) {
            r->on_request(r);
        },
        g_fiy->config.concurrency
    };

    // Send it to the thread pool
    request_handler.emplace(new ModDllConnectorRequest(std::move(conn), user, m_mod_info->on_request));
}

void ModConnectorDll::handle_request(const fiy::fiy_request_t* req) {
    // Call request handler directly in same thread
    m_mod_info->on_request(req);
}
