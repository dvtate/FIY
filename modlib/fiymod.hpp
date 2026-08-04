//
// Created by tate on 7/15/25.
//

#ifndef FIY_FIYMOD_HPP
#define FIY_FIYMOD_HPP

#include <string>
#include <string_view>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>

#include <unistd.h>

#include "fiymod.h"

namespace fiy {
    using ModInfo = fiy_mod_info_t;
    using BodyType = fiy_body_type;
    using StartFunction = fiy_mod_start_function_t;

    /**
     * Type for response bodies
     * @note for string/buffer bodies, this operates like a string_view
     *      the passed in data shouldn't get free'd until after callback/respond called
     */
    struct Body : public fiy_body_t {
        using Type = fiy_body_type;
        Body(): fiy_body_t({
            .type = FIY_BODY_NONE,
            .buffer = { nullptr, 0 }
        }) {}
        explicit Body(FILE* f): fiy_body_t({
            .type = Type::FIY_BODY_FILE,
            .file = { .fd= fileno(f), .offset = ftell(f) }
        }) {}
        explicit Body(const int file_descriptor, const long offset = 0):
            fiy_body_t({
                .type = Type::FIY_BODY_FILE,
                .file = { .fd = file_descriptor, .offset = offset }
            })
        {}

        Body(const fiy_buffered_reader_fn fn, void* context): fiy_body_t({
            .type = FIY_BODY_READER,
            .reader = { .read = fn, .context = context }})
        {}

        explicit Body(const std::string_view body): fiy_body_t({
            .type = body.empty() ? FIY_BODY_NONE : FIY_BODY_BUFFER,
            .buffer = { body.data(), body.length() }
        }) {}
        explicit Body(const char* buffer): fiy_body_t({
            .type = FIY_BODY_BUFFER,
            .buffer = { .data = buffer, .length = strlen(buffer) }
        }) {}
        Body(const char* buffer, const size_t length): fiy_body_t({
            .type = FIY_BODY_BUFFER,
            .buffer = { .data = buffer, .length = length }
        }) {}
        Body(std::string&& body) = delete;
            // this structure works like a string_view.

        /**
         * Convert the body into a string
         * @param body body to convert to a string
         * @return string representation of the body
         * @remark after this is called the body should not be used again
         */
        [[nodiscard]] static std::string to_string(const fiy_body_t& body) {
            switch (body.type) {
            case FIY_BODY_NONE:
                return "";

            case FIY_BODY_BUFFER:
                if (body.buffer.data == nullptr) {
#ifdef FIY_DEBUG
                    return "FIY_BODY_BUFFER data is NULL";
#else
                    return "";
#endif
                }
                return std::string(body.buffer.data, body.buffer.length);

            case FIY_BODY_READER: {
                std::string ret;
                char buff[8192];
                for (;;) {
                    const auto n = body.reader.read(body.reader.context, buff, 8192);
                    if (n > 0)
                        ret.append(buff, n);
                    else
                        return ret;
                }
            }

            case FIY_BODY_FILE: {
                std::string ret;
                char buff[8192];
                for (;;) {
                    const auto n = read(body.file.fd, buff, sizeof buff);
                    if (n > 0)
                        ret.append(buff, n);
                    else
                        break;
                }
                close(body.file.fd); // prevent fd leak
                return ret;
            }

            // invalid
            default:
                std::cerr <<"fiy::Body::to_string(): Invalid body type!!\n";
                // std::unreachable();
                return "";
            }
        }

        /**
         * Read contents of body into string
         * @return body as a string
         */
        [[nodiscard]] std::string to_string() const {
            return to_string(*this);
        }
    };

    struct Response : public fiy_response_t {
    protected:
        std::string m_headers;

    public:
        Response():
            fiy_response_t({
                .status = 200,
                .headers = nullptr,
                .body = Body()
            })
        {}

        explicit Response(
            const int status,
            std::string headers_str = "",
            const Body& body = Body()
        ):
            m_headers(std::move(headers_str))
        {
            this->status = status;
            this->headers = m_headers.c_str();
            this->body = body;
        }

        Response(const Response& other) {
            this->status = other.status;
            this->body = other.body;
            m_headers = other.m_headers;
            headers = m_headers.c_str();
        }

        void add_header(const std::string_view field, const std::string_view value) {
            m_headers += field;
            m_headers += ": ";
            m_headers += value;
            m_headers += '\n';
            headers = m_headers.c_str();
        }
        void add_header(const std::string_view header) {
            m_headers += header;
            m_headers += '\n';
            headers = m_headers.c_str();
        }

        void clear_headers() {
            m_headers.clear();
            headers = nullptr;
        }

        void set_headers(const std::string_view headers_str) {
            m_headers = headers_str;
            headers = m_headers.c_str();
        }
        void set_headers(std::string&& headers_str) {
            m_headers = std::move(headers_str);
            headers = m_headers.c_str();
        }

#if 0
        ~Response() {
            clear_headers();
        }
#endif
    };



    // TODO maybe make this a namespace instead?
    struct Host : public fiy_host_info_t {
        Host() = delete; // deleted to prevent typos w/ fiy::host()
        Host(const fiy_host_info_t& hi): fiy_host_info_t(hi) {}

        [[nodiscard]] std::string host_base_uri() const {
            static const std::string protocol =
                base_uri[4] == 's' ? "https://" : "http://";
            return protocol + domain;
        }

        /**
         * Write a log
         * @param type log level
         *  - 0 : fatal
         *  - 1 : error
         *  - 2 : warning
         *  - 3 : info
         *  - 4 : debug
         * @param msg log message text
         */
        // void log(
        //     const int type,
        //     const std::string& msg,
        //     const std::string& file = __builtin_FILE(),
        //     const std::string& line = __builtin_LINE()
        // ) {
        //     std::string msg2 = this->appid;
        //     msg2 += ": "
        //     msg2 += file;
        //     msg2 += ":";
        //     msg2 += line;
        //     msg2 += " - ";
        //     msg2 += msg;
        //     fiy_host_info_t::log(type, msg2.c_str());
        // }
        void log(const int type, const std::string& msg) const {
            fiy_host_info_t::log(type, msg.c_str());
        }

        enum class Log : int {
            FATAL = 0,
            ERROR = 1,
            WARNING = 2,
            INFO = 3,
            DEBUG = 4,
        };

        /**
         * Write a log
         * @param type log type/severity
         * @param msg log message text
         */
        void log(const Log type, const std::string& msg) const {
            log(static_cast<int>(type), msg);
        }
        void log_fatal(const std::string& msg) const {
            log(Log::FATAL, msg);
        }
        void log_error(const std::string& msg) const {
            log(Log::ERROR, msg);
        }
        void log_warning(const std::string& msg) const {
            log(Log::WARNING, msg);
        }
        void log_info(const std::string& msg) const {
            log(Log::INFO, msg);
        }
        void log_debug(const std::string& msg) const {
            log(Log::DEBUG, msg);
        }

        /**
         * Get components from user string
         * @param user user string of format user@domain
         * @return pair < user, domain > of same format
         */
        [[nodiscard]] std::pair<std::string_view, std::string_view>
        split_user_str(const std::string_view user) const {
            // No user
            if (user.empty())
                return std::make_pair("","");

            // Local user
            const auto at_idx = user.find('@');
            if (at_idx >= user.size() - 1 // includes "user@" and "user"
                || user.substr(at_idx + 1) == this->domain
            )
                return std::make_pair(user.substr(0, at_idx), "");

            // Remote user
            return std::make_pair(
                user.substr(0, at_idx),
                user.substr(at_idx + 1)
            );
        }

        /**
         * Send a request to another mod
         * @param mod_id app to send the request to
         * @param request request to send to the other mod
         *      method   - http method
         *      path     - uri path
         *      domain   - remote server to send request to or nullptr if local request
         *      user     - local user or nullptr if unauthenticated
         * @param callback called with response
         * @remark this is a wrapper around the host-provided "request" member
         * @notes
         * - local apps can send requests to each other without restrictions
         * - an app on server a can only send requests to apps on server b on behalf of users residing on server a
         *    - this prevents false impersonation
         */
        // TODO change this, should include request in cb so caller can delete it
        void request_mod(
            const char* mod_id,
            struct fiy_request_t* request,
            std::function<void(const Response*)> callback
        ) const {
            // Check params
            if (request == nullptr || mod_id == nullptr || callback == nullptr)
                return;

            // Check host property
            if (this->request == nullptr)
                return;

            // Call host implementation
            auto* cb = new std::function{std::move(callback)};
            request->context = static_cast<void*>(cb);
            request->callback = [](const fiy_request_t* req, const fiy_response_t* res) {
                const auto* cb_fn = static_cast<const std::function<void(const Response*)>*>(req->context);
                (*cb_fn)(static_cast<const Response*>(res));
                delete cb_fn;
            };
            this->request(mod_id, request);
        }
    };

    /// Mod-local singleton
    __attribute__((visibility("hidden")))
    inline Host& host() {
        static Host static_host_info{fiy_host_info_t{
            .domain = nullptr,
            .app_id = nullptr,
            .base_uri = nullptr,
            .data_dir = nullptr,
            .log = nullptr,
            .request = nullptr,
            .local_login = nullptr,
            .user_info = nullptr,
            .now = nullptr,
            .mod_config = nullptr
        }};
        return static_host_info;
    }

    /// Degrees of separation
    enum class Locality {
        USER = 0,       /// Request is from the user
        INSTANCE = 1,   /// Request is from another user on same instance
        FEDIVERSE = 2,  /// Request is from another user in the fediverse
        PUBLIC = 3      /// Request is anonymous
    };

    // Must not add members or else not pointer compatible
    struct Request : public fiy_request_t {
        using Callback = decltype(fiy_request_t::callback);

        // match those from boost::beast::http::verb
        enum Method : uint8_t {
            UNKNOWN = 0,
            DELETE, GET, HEAD, POST, PUT, CONNECT, OPTIONS,
            TRACE, COPY, LOCK, MKCOL, MOVE, PROPFIND, PROPPATCH,
            SEARCH, UNLOCK, BIND, REBIND, UNBIND,
            ACL, REPORT, MKACTIVITY, CHECKOUT, MERGE, MSEARCH,
            NOTIFY, SUBSCRIBE, UNSUBSCRIBE, PATCH, PURGE,
            MKCALENDAR, LINK, UNLINK
        };

        explicit Request(
            const char* path,
            const uint8_t method = (uint8_t) Method::GET,
            const char* domain = nullptr,
            const char* user = nullptr,
            const char* headers = nullptr,
            const char* body = nullptr,
            const size_t body_len = 0,
            const Callback callback = nullptr,
            void* context = nullptr
        ): fiy_request_t{
            .domain=domain,
            .user=user,
            .method=method,
            .body_len=body_len,
            .path=path,
            .headers=headers,
            .body=body,
            .callback = callback,
            .context = context
        } {
        }

        Request(const fiy_request_t& req): fiy_request_t(req) {}

        /**
         * Get user string for request user
         * @param anon_name return value when anonymous user
         * @return user string, either username for local user or username + @ + domain for federated user
         */
        [[nodiscard]] std::string user_str(const std::string& anon_name = "") const {
            if (user == nullptr)
                return anon_name;
            if (domain == nullptr)
                return user;
            return std::string(user) + "@" + std::string(domain);
        }

        /// User string that always includes the host, even for local users
        /// ie - user@example.com
        [[nodiscard]] std::string global_user_str() const {
            if (user == nullptr)
                return "";
            std::string ret = user;
            ret += '@';
            ret += domain == nullptr
                ? fiy::host().domain
                : domain;
            return ret;
        }

        [[nodiscard]] const char* method_str() const {
            return fiy_http_verb_strings[method];
        }
        [[nodiscard]] static const char* method_str(const uint8_t method) {
            return fiy_http_verb_strings[method];
        }
        [[nodiscard]] bool is_local() const {
            return user != nullptr && domain == nullptr;
        }

        inline void respond(const fiy_response_t& resp) const {
            this->callback(this, &resp);
        }

        /**
         * Respond to request
         * @param cb provided callback function
         * @param status HTTP status code
         * @param headers headers string
         * @param body HTTP body
         */
        void respond(
            const int status = 200,
            const std::string& headers = "",
            const Body& body = Body()
        ) const {
            const fiy_response_t res = {
                .status = status,
                .headers = headers.empty() ? nullptr : headers.c_str(),
                .body = body,
            };
            this->callback(this, &res);
        }
        void respond(
            const int status,
            const std::string& headers,
            const std::string& body
        ) const {
            const fiy_response_t res = {
                .status = status,
                .headers = headers.empty() ? nullptr : headers.c_str(),
                .body = Body(body),
            };
            this->callback(this, &res);
        }

        /**
         * Get the trust level of a given request wrt. a local user
         * @param local_user local user this request wants information on
         * @return Locality trust level
         */
        [[nodiscard]] Locality locality(const std::string& local_user) const {
            return user == nullptr
                ? Locality::PUBLIC
                : domain == nullptr
                    ? (local_user != user ? Locality::INSTANCE : Locality::USER)
                    : Locality::FEDIVERSE;

            // TODO maybe a use case for local app-to-app requests
            //      in this case the user would be null and domain would be host domain
            //      this would probably mean privilege level -1
        }

        /**
         * Get the first value for a give http header
         * @param key HTTP header key to find
         * @return First value associated with given header
         */
        [[nodiscard]] std::string_view find_header(const std::string_view key) const {
            if (this->headers == nullptr)
                return "";

            std::string_view fields = this->headers;
            std::size_t start = 0;
            while (start < fields.size()) {
                // Find colon
                std::size_t end = fields.find(':', start);
                if (end == std::string_view::npos)
                    return "";

                // Check if key length matches
                if ((end - start) == key.size()) {

                    // Check if key value matches
                    for (size_t i = 0; i < end - start; i++)
                        if (tolower(fields[start + i]) != tolower(key[i]))
                            goto next_field;

                    // Skip ": "
                    end++;
                    while (end < fields.size() && isspace(fields[end]))
                        end++;

                    // Return corresponding value
                    std::size_t endl = fields.find_first_of("\r\n", end);
                    if (endl == std::string_view::npos)
                        endl = fields.size();
                    return fields.substr(end, endl - end);
                }

                // Check next field
next_field:
                start = fields.find('\n', end);
                if (start == std::string_view::npos)
                    return "";
                start++; // skip newline
            }

            return "";
        }

        /**
         * Parse the headers into a map where all the keys are lower-case
         * @return headers lookup map
         */
        template <class MapT = std::map<std::string, std::string_view>>
        [[nodiscard]] MapT headers_map() const {
            MapT ret;

            if (this->headers == nullptr)
                return ret;

            std::string_view fields = this->headers;
            std::size_t start = 0;
            std::string key;
            key.reserve(32);
            do {
                // Find colon
                std::size_t end = fields.find(':', start);
                if (end == std::string_view::npos)
                    return ret;

                // Convert key to lower case
                key.clear();
                for (size_t i = 0; i < end - start; i++)
                    key += static_cast<char>(tolower(fields[start + i]));

                // Find next start
                start = fields.find('\n', end);
                if (start == std::string_view::npos)
                    start = fields.size();

                // Put it into the map
                end++;
                if (fields[end] == ' ')
                    end++;
                auto v = fields.substr(end, start - end);
                if (v.back() == '\r')
                    v.remove_suffix(1);
                ret.emplace(key, v);

                // Check next field
                start++; // after \n
            } while (start != std::string_view::npos);

            return ret;
        }

        /**
         * Get a map of the query parameters
         * @return
         */
        template <class MapT = std::map<std::string_view, std::string_view>>
        [[nodiscard]] MapT query_params() const {
            // Find query string
            MapT ret;
            const char* qs = strchr(path, '?');
            if (qs == nullptr)
                return ret;

            // Parse query string
            std::string_view q = qs;
            while (!q.empty()) {
                if (q[0] == '&' || q[0] == '?') {
                    q.remove_prefix(1);
                }

                // Parse query param key + value
                auto eq = q.find_first_of("=&");
                std::string_view key;
                std::string_view value;
                if (eq == std::string::npos) {
                    key = q;
                    value = "";
                    q = "";
                } else if (q[eq] == '=') {
                    key = q.substr(0, eq);
                    auto end = q.find('&', eq);
                    value = q.substr(eq + 1, end == std::string::npos
                        ? end : (end - eq - 1));
                    q.remove_prefix(end);
                } else if (q[eq] == '&') {
                    key = q.substr(0, eq);
                    value = "";
                    q.remove_prefix(eq);
                }
                ret.emplace(key, value);
            }
            return ret;
        }

    };

    template <>
    [[nodiscard]] inline std::vector<std::pair<std::string_view, std::string_view>>
    Request::query_params() const {

        // Find query string
        std::vector<std::pair<std::string_view, std::string_view>> ret;
        const char* qs = strchr(path, '?');
        if (qs == nullptr)
            return ret;

        // Parse query string
        std::string_view q = qs;
        while (!q.empty()) {
            if (q[0] == '&' || q[0] == '?') {
                q.remove_prefix(1);
            }

            // Parse query param key + value
            auto eq = q.find_first_of("=&");
            std::string_view key;
            std::string_view value;
            if (eq == std::string::npos) {
                key = q;
                value = "";
                q = "";
            } else if (q[eq] == '=') {
                key = q.substr(0, eq);
                auto end = q.find('&', eq);
                value = q.substr(eq + 1, end == std::string::npos
                    ? end : (end - eq - 1));
                q.remove_prefix(end);
            } else if (q[eq] == '&') {
                key = q.substr(0, eq);
                value = "";
                q.remove_prefix(eq);
            }
            ret.emplace_back(key, value);
        }
        return ret;
    }

    /**
     * Wrapper for FIY users
     * @tparam TU type of the username field
     * @tparam TD type of the domain field
     */
    template <typename TU, typename TD = const char*>
    struct User {
        TU user;
        TD domain;

        User() = default;
        explicit User(const TU& user, const TD& instance_domain = TD()):
            user(user), domain(instance_domain)
        {}

        [[nodiscard]] std::string to_string(const bool local_domain = false) const {
            std::string ret;
            if (user)
                ret += user;

            // User specified domain
            if (domain) {
                ret += '@';
                ret += domain;
                return ret;
            }

            // Explicitly include instance domain
            if (local_domain) {
                ret += '@';
                ret += fiy::host().domain;
            }
            return ret;
        }

        template <typename TU2, typename TD2>
        Locality distance(const User& other) const {
            // Not authenticated
            if (!user || !other.user)
                return Locality::PUBLIC;

            // Same domain
            const std::string_view hd = fiy::host().domain;
            if (domain == other.domain
                || (!domain && other.domain == hd)
                || (!other.domain && domain == hd)
            )
                return user == other.user
                    ? Locality::USER
                    : Locality::INSTANCE;

            // Different domain
            return Locality::FEDIVERSE;
        }
    };

    // Users may want to make PPC wrappers around these that include file/line numbers
    /**
     * Write a log
     * @param type log type/severity
     * @param msg log message text
     */
    inline void log(const Host::Log type, const std::string& msg) {
        static const std::string prefix = std::string(host().app_id) + ": ";
        // assert(
        //     host().fiy_host_info_t::log != nullptr,
        //     "you should call fiy::Host::set(host_info) in start(host_info)"
        // );
        host().fiy_host_info_t::log(
            static_cast<int>(type),
            (prefix + msg).c_str());
    }
    inline void log_fatal(const std::string& msg) {
        log(Host::Log::FATAL, msg);
    }
    inline void log_error(const std::string& msg) {
        log(Host::Log::ERROR, msg);
    }
    inline void log_warning(const std::string& msg) {
        log(Host::Log::WARNING, msg);
    }
    inline void log_info(const std::string& msg) {
        log(Host::Log::INFO, msg);
    }
    inline void log_debug(const std::string& msg) {
        log(Host::Log::DEBUG, msg);
    }
} // namespace fiy

using fiy::fiy_request_t;
using fiy::fiy_response_t;
using fiy::fiy_host_info_t;

#endif //FIY_FIYMOD_HPP
