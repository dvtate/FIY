//
// Created by tate on 12/3/25.
//

// TODO PoC PoC -- Piece of crap proof of concept
//      needs cleaning up

#pragma once

#include <cstring>
#include <string_view>
#include <string>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

#include <boost/beast/core/detail/base64.hpp>

#include "../../../../util/CGI.hpp"
#include "../../../../util/FileCache.hpp"
#include "../../../../modlib/fiymod.hpp"

/*
 * This file implements a wrapper around the git-http-backend CGI
 * This is a quick and dirty solution that has lots of room for
 * optimizations. Eventually this could be replaced a more
 * performant custom solution.
 */

// inline std::string find_git_http_backend() {
//     // Look check all possible paths
//     for (const std::string& p : {
//         "/usr/lib/git-core/git-http-backend"
//     })
//         if (std::filesystem::exists(p)) {
//             fiy::host().log(4, "Found CGI plugin: " + p);
//             return p;
//         }
//
//     // Not found
//     fiy::host().log(0, "Could not find git-http-backend, are you sure git is installed?");
//     throw std::runtime_error("Could not find git-http-backend, are you sure git is installed?");
// }

inline std::string get_repo_path(const std::string_view path) {
    std::string ret = fiy::host().data_dir;
    ret += "/repos";
    const auto i = path.find(".git");
    if (i == std::string::npos) {
        fiy::log_error("invalid repo path? " + std::string(path));
        ret += path;
        return ret;
    }
    ret += path.substr(0, i + 4);
    return ret;
}

/**
 * Get the CGI params from the request URI path
 * @param path (in) request URI path
 * @param SERVER_PROTOCOL (out)
 * @param SERVER_NAME (out)
 * @param SERVER_PORT (out)
 * @param PATH_INFO (out)
 * @param QUERY_STRING (out)
 */
inline void get_uri_components(
    const std::string_view path,
    std::string& SERVER_PROTOCOL,
    std::string& SERVER_NAME,
    std::string& SERVER_PORT,
    std::string& PATH_INFO,
    std::string& QUERY_STRING
) {
    // Protocol
    const bool https = fiy::host().base_uri[4] == 's';
    SERVER_PROTOCOL = https ? "https" : "http";

    // Domain + Port
    std::string hostname = fiy::host().domain;
    const auto port_sep = hostname.find(':');
    SERVER_NAME = hostname.substr(0, port_sep);
    if (port_sep != std::string::npos) {
        SERVER_PORT = hostname.substr(
            port_sep + 1
            // , hostname.find('/', port_sep) - port_sep // This shouldn't be needed
        );
    } else {
        SERVER_PORT = https ? "443" : "80";
    }

    // Query string
    const auto qs = path.find('?');
    if (qs != std::string_view::npos)
        QUERY_STRING = path.substr(qs + 1);

    // PATH_INFO
    // not sure about logic here tbh
    std::string_view subpath = fiy::host().base_uri + (https ? 8 : 7); // skip protocol
    subpath.remove_prefix(subpath.find('/')); // skip domain: [example.com]/git/user/repo/...
    if (subpath.empty() || subpath == "/") {
        PATH_INFO = path.substr(0, qs);
    } else {
        PATH_INFO = subpath;
        PATH_INFO += path;
    }

    // Server stuff
    SERVER_NAME = fiy::host().domain;
}

inline size_t parse_cgi_headers(const std::string_view s, fiy::Response& ret) {
    // Parse headers
    size_t start = 0;
    size_t end;
    do {
        end = s.find("\r\n", start);
        auto line = s.substr(start, end - start);
        if (line.empty()) { // empty header
            start = end + 2;
            break;
        }
        const auto i = line.find(':');
        if (i == std::string::npos) {
            std::cerr <<"WTF? invalid header? --" <<line << std::endl;
        }

        // Read the fields
        if (line.size() > 6 && strncasecmp(line.data(), "Status", 6) == 0) {
            // This isn't a valid http header so we gotta handle it differently
            // Status         = "Status:" status-code SP reason-phrase NL
            const char* value = line.data() + i + 1;
            char* end_str;
            ret.status = strtol(value, &end_str, 10);
            if (ret.status == 0 && end_str == value)
                ret.status = 500;
            // std::cout <<" Status: " << ret.status << std::endl;
        } else {
            ret.add_header(line);
            // std::cout <<"Header: " << line << std::endl;
        }
        start = end + 2; // \r\n
    } while (end != std::string::npos && start < s.size() && s[start] != '\n');

    return end == std::string::npos ? 0 : start;
}

inline fiy::Response parse_cgi_output(const std::string& s) {
    fiy::Response ret;

    // Parse headers
    size_t start = 0;
    size_t end;
    size_t body_len = 0;
    do {
        end = s.find("\r\n", start);
        auto line = s.substr(start, end - start);
        if (line.empty()) {
            start = end + 2;
            break;
        }
        const auto i = line.find(':');
        if (i == std::string::npos) {
            std::cerr <<"WTF? invalid header? --" <<line << std::endl;
        }

        // Do something with the header
        auto header = line.substr(0, i);
        std::ranges::transform(header, header.begin(),
            [](const auto c){ return std::tolower(c); });
        if (header == "status") {
            // This isn't a valid http header so we gotta handle it differently
            // Status         = "Status:" status-code SP reason-phrase NL
            const char* value = line.data() + i + 1;
            char* str_end;
            ret.status = strtol(value, &str_end, 10);
            if (ret.status == 0 && str_end == value)
                ret.status = 500;
            // std::cout <<" Status: " << ret.status << std::endl;
        } else if (header == "content-length") {
            // Extra Validation later
            const char* value = line.data() + i + 1;
            char* str_end;
            body_len = strtol(value, &str_end, 10);
            ret.add_header(line);
            // std::cout <<" Content-length" << ret.body_len << std::endl;
        } else {
            ret.add_header(line);
            // std::cout <<"Header: " << line << std::endl;
        }
        start = end + 2; // \r\n
    } while (end != std::string::npos && start < s.size() && s[start] != '\n');

    // Extract body
    if (end != std::string::npos) {
        // Body length
        const size_t body_len2 = s.size() - start;
        ret.body = fiy::Body(s.c_str() + start, body_len2);
        if (body_len != 0 && body_len != body_len2) {
            fiy::log_warning("Incorrect Content-Length header");
        }
    }

    return ret;
}

inline fiy::Response parse_cgi_output(const int fd) {
    // TODO
    // Read string until \r\n\r\n
    return {};
}

/**
 * Wrapper around the git-http-backend CGI plugin included with git
 */
inline void git_repo_cgi(const fiy::Request& req) {
    std::string path{req.path};

    CGI cgi{{"git", "http-backend" }};

    cgi.REQUEST_METHOD = req.method_str();
    if (req.body != nullptr) {
        cgi.CONTENT_LENGTH = std::to_string(req.body_len);
        cgi.body = std::string(req.body, req.body_len);
    }
    cgi.set_env("GIT_HTTP_EXPORT_ALL", "1"); // not accepting raw paths so no worries
    cgi.set_env("GIT_HTTP_MAX_REQUEST_BUFFER", "1000M");

    get_uri_components(path,
        cgi.SERVER_PROTOCOL,
        cgi.SERVER_NAME,
        cgi.SERVER_PORT,
        cgi.PATH_INFO,
        cgi.QUERY_STRING);

    cgi.PATH_TRANSLATED = concat(
        fiy::host().data_dir,
        "/repos",
        path.substr(0, path.find('?'))
    );

    // TODO instead manually set name + email combo?
    cgi.REMOTE_ADDR = req.domain ? req.domain : fiy::host().domain;
    cgi.REMOTE_HOST = "NULL";
    cgi.REMOTE_USER = req.user_str();

    // Add other http headers altho probably not needed
    // std::cout <<"Headers string: "
    //     << (req.headers == nullptr ? "null" : req.headers)
    //     << "\n=========================\n";
    for (const auto& [h , v] : req.headers_map()) {
        if (h == "content-type") {
            cgi.CONTENT_TYPE = v;
            continue;
        }
        // if (h == "content-length") {
        //     if (v != cgi.CONTENT_LENGTH) {
        //         std::cerr <<"Content length mismatch!!!\n";
        //     }
        //     cgi.CONTENT_LENGTH = v;
        // }
        std::string ev = "HTTP_";
        ev += h;
        for (size_t i = 4; i < ev.size(); i++)
            if (ev[i] == '-')
                ev[i] = '_';
            else
                ev[i] = std::toupper(ev[i]);
        cgi.set_env(ev, std::string(v));
        // std::cerr << "Set header env: '" <<ev <<"' = '" << v << "'\n";
    }

    // FILE* f = tmpfile();
    // if (f == nullptr) {
        auto r = cgi.run();
        if (r.status != 0) {
            fiy::log_error("git http-backend cgi failed");
            req.respond(500, "Git CGI failed");
            // std::cerr <<"\nstatus: " << r.status << std::endl;
            // std::cerr <<"\nstdout: " << r.stdout << std::endl;
            // std::cerr <<"err!\n";
            //
            // for (auto& e : cgi.get_env())
            //     std::cout <<"env: '" <<e <<"'\n";
            //
            // std::cerr << "\n====================================\nBody:\n";
            // int fd = open("/tmp/dbg.data", O_CREAT | O_RDWR, 0666);
            // if (write(fd, cgi.body.data(), cgi.body.size()) < 0)
            //     perror("write()");
            // std::cerr << cgi.body << "\n====================================\n";
            return;
        }

        // std::cout << "--------------------------------------------\n";
        // std::cout << "\nCGI Result: " <<r.stdout << std::endl;
        // std::cout << "--------------------------------------------\n";
        req.respond(parse_cgi_output(r.stdout));
        return;
    // }
    // auto r = cgi.run(fileno(f));
    // Parse header
    // Convert into response object
    // With file body

}

/*
Requests:

// git pull
GET -- /git/test/test.git/info/refs?service=git-upload-pack
POST -- /git/test/test.git/git-upload-pack
POST -- /git/test/test.git/git-upload-pack

// git push
GET -- /git/test/test.git/info/refs?service=git-receive-pack
POST -- /git/test/test.git/git-receive-pack
*/