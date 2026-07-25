//
// Created by tate on 2/20/26.
//

#include <fcntl.h>

#include <filesystem>
#include <fstream>
#include <optional>

#include "../../util/FileCache.hpp"

#if defined __has_include
#  if __has_include (<linux/limits.h>)
#    include <linux/limits.h>
#  endif
#endif

#include <nlohmann/json.hpp>

#include "../../modlib/fiymod.hpp"
#include "../../util/mime.hpp"
#include "../../util/WebUtils.hpp"

namespace fs = std::filesystem;

/// Where to look for the files
static fs::path g_static_root;

/// Index files to use when directory requested
static std::vector<std::string> g_index_files{
    "index.html",
    "index.htm",
    "index.xhtml",
};

/// If a directory is chosen and it doesn't contain an index file,
/// should we list the contents of the directory?
static bool g_list_directory = true;

/// Should the server and client cache files?
static bool g_cache = false;

/// Pre-cached root index
// static MMFile* g_cached_index_file = nullptr;

/// Make response for file
static fiy::Response file_response(const fs::path& path) {
    // Respond with the file
    // fiy::host().log_info("Requested file: " + path->string());
    fiy::Response res;
    const auto fd = open(path.c_str(), O_RDONLY);
    if (fd == -1) [[unlikely]] {
        fiy::host().log_error("Could not open file " + path.string() + " : " + strerror(errno));
        res.status = 500;
        res.headers = "Content-Type: text/html";
        res.body = fiy::Body("<h1>Server Error</h1> <p>Could not open file!</p>");
        return res;
    }

    // posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
    static const std::string headers_prefix = g_cache
        ? "Cache-Control: max-age=600\nContent-Type: "
        : "Content-Type: ";
    res.body = fiy::Body(fd, 0);
    res.status = 200;
    res.set_headers(headers_prefix + std::string(get_ext_mime_type(path.extension().string().substr(1))));
    return res;

    // FIY Host closes the file when it's done. Don't close it here.
    // No need to close it here
    // if (close(fd) != 0)
    //     fiy::host().log_error("failed to close() file: " + f->string());
}

static std::string list_directory_body(const fs::path& path) {
    const std::string relative_path = fs::relative(path, g_static_root).string();

    // std::cout <<"relative( " <<g_static_root <<", " <<path <<") = " <<relative_path <<std::endl;;
    std::string body = concat(
        "<!DOCTYPE html><html><head><title>Index of ",
        relative_path,
        "</title><base href=\"",
        fiy::host().base_uri,
        '/',
        relative_path,
        "/\"></head><body><h1>Index of ",
        relative_path,
        "/</h1><hr><pre>"
    );

    for (const auto& entry : fs::directory_iterator(path)) {
        std::string p = entry.path().filename().string();
        if (entry.is_directory()) {
            body += concat(
                "<a href=\"./",
                WebUtils::uri_encode(p),
                "/\">",
                p,
                "/</a>\n"
            );
        } else {
            body += concat(
                "<a href=\"./",
                WebUtils::uri_encode(p),
                "\">",
                p,
                "</a>\n"
            );
        }
    }

    body += "</pre>\n<!-- set mod_settings.list_directories to false to disable this page --></body></html>";
    return body;
}

static std::string get_error_404_body() {
    std::string ret = load_file_as_string(g_static_root / "404.html");
    if (!ret.empty())
        return ret;

    ret = concat("<h1>404: Not Found</h1>"
        "<p>The page or file you are looking for could not be found."
        " If you were trying to use an app it's possible that it is not installed or has a different path</p>"
        "<hr/><a href=\"//",
        fiy::host().domain,
        "\">",
        fiy::host().domain,
        "</a> | <a href=\"//",
        fiy::host().domain,
        "/portal\">FIY Portal</a>\n\n<!-- to use a custom 404 page create a 404.html file -->"
    );
    return ret;
}

static void handle_request(fiy::Request& req, const fiy::fiy_callback_t cb) {
    try {
        static const std::string body_404 = get_error_404_body();
        static const fiy::fiy_response_t response_404{
            .status = 404,
            .headers = g_cache
                ? "Cache-Control: max-age=600\nContent-Type: text/html"
                : "Content-Type: text/html",
            .body = fiy::Body(body_404)
        };
        static const fiy::fiy_response_t response_403{
            .status=403,
            .headers = g_cache
                ? "Cache-Control: max-age=600\nContent-Type: text/html"
                : "Content-Type: text/html",
            .body = fiy::Body("Error 403 - Forbidden")
        };

        // Strip query string and fragment
        const auto decoded_path = WebUtils::uri_decode(req.path, strlen(req.path));
#ifdef PATH_MAX
        if (decoded_path.size() >= PATH_MAX) {
            req.respond(cb, 400, "Content-Type: text/html",
                fiy::Body("Error 400: Path Length too long"));
            return;
        }
#endif
        std::string_view url_path = decoded_path;
        auto pos = url_path.find_first_of("?#");
        if (pos != std::string::npos)
            url_path = url_path.substr(0, pos);

        // Remove leading '/'
        pos = url_path.find_first_not_of('/');
        if (pos != std::string::npos)
            url_path = url_path.substr(pos);
        else
            url_path = "";

        // // Cached index file (probably overkill)
        // if (url_path.empty() && g_cache) {
        //     req.respond(cb, 200,
        //     "Cache-Control: max-age=600\nContent-Type: text/html",
        //         fiy::Body(g_cached_index_file->data(), g_cached_index_file->size()));
        //     return;
        // }

        // Weakly canonical resolves ".." even if file doesn't exist
        fs::path normalized = fs::weakly_canonical(g_static_root / url_path);

        // Ensure path is still inside base directory
        const auto mismatch = std::mismatch(
            g_static_root.begin(), g_static_root.end(),
            normalized.begin(), normalized.end()
        ).first;
        if (mismatch != g_static_root.end()) {
            // fiy::host().log_debug("Directory escape attempted: " + std::string(normalized));
            req.respond(cb, response_403);
            return;
        }

        // File doesn't exist
        if (!fs::exists(normalized)) {
            // fiy::host().log_debug("File does not exist: " + std::string(normalized));
            req.respond(cb, response_404);
            return;
        }

        // If it's a directory, try index files
        if (fs::is_directory(normalized)) {
            for (const auto& index : g_index_files) {
                fs::path index_path = normalized / index;
                if (fs::exists(index_path) && fs::is_regular_file(index_path)) {
                    req.respond(cb, file_response(index_path));
                    return;
                }
            }

            if (g_list_directory) {
                const std::string body = list_directory_body(normalized);
                req.respond(cb, 200, "Content-Type: text/html", fiy::Body(body));
                return;
            }

            req.respond(cb, response_404);
            return;
        }

        // Must exist and be a regular file
        if (fs::is_regular_file(normalized)) {
            req.respond(cb, file_response(normalized));
            return;
        }

        // Maybe a pipe or fifo... let's just pretend it doesn't exist
        req.respond(cb, response_404);
        return;
    } catch (const fs::filesystem_error& e) {
        std::string body = concat("<h1>Server Error</h1>Filesystem error: ", e.what());
        req.respond(cb, 500, "Content-Type: text/html", fiy::Body(body));
        return;
    }
}

/// Export: Start module
FIY_EXPORT fiy::ModInfo* start(const fiy_host_info_t* host_info) {
    fiy::host() = *host_info;

    // Read config
    std::ifstream ifs{fiy::host().mod_config};
    auto config = nlohmann::json::parse(ifs);
    if (!config.contains("mod_settings") || !config["mod_settings"].is_object()) {
        fiy::host().log_fatal("module.json: expected a mod_settings field containing an object with target path");
        return nullptr;
    }
    config = config["mod_settings"];
    if (!config.contains("root") || !config["root"].is_string()) {
        fiy::host().log_fatal("module.json: expected a mod_settings 'root' field set to string path to static root");
        return nullptr;
    }
    g_static_root = config["root"].get<std::string>();
    if (g_static_root.is_relative())
        g_static_root = fiy::host().data_dir / g_static_root;

    try {
        g_static_root = std::filesystem::absolute(g_static_root);
        if (!std::filesystem::exists(g_static_root)) {
            fiy::host().log_fatal("module.json: target path does not exist: " + std::string(g_static_root));
            return nullptr;
        }
    } catch (const std::filesystem::filesystem_error& e) {
        fiy::host().log_fatal("module.json: Invalid target "
            + std::string(g_static_root) + ": " + e.what());
        return nullptr;
    }
    if (config.contains("index")) {
        if (!config["index"].is_array()) {
            fiy::host().log_error("module.json: mod_settings 'index' field should be an array of index filename strings");
        } else {
            try {
                g_index_files = config["index"].get<std::vector<std::string>>();
            } catch (...) {
                fiy::host().log_error("module.json: Invalid mod_settings.index field");
            }
        }
    }
    if (config.contains("list_directories")) {
        if (!config["list_directories"].is_boolean()) {
            fiy::host().log_error("module.json: list_directories should be boolean");
        } else {
            g_list_directory = config["list_directories"].get<bool>();
        }
    }
    if (config.contains("cache")) {
        try {
            g_cache = config["cache"].get<bool>();
            // if (g_cache) {
            //     for (const auto& f : g_index_files) {
            //         try {
            //             g_cached_index_file = new MMFile(g_static_root / f);
            //         } catch (const MMFile::Error& e) {
            //             // C++ automatically frees it
            //             fiy::host().log_info("pre-caching root index: " + std::string(e.what()));
            //         }
            //     }
            // }
        } catch (...) {
            fiy::host().log_error("module.json: Expected mod_settings.cache to be a boolean");
        }
    }

    // Exchange info with host
    static fiy::ModInfo mod_info = {
        .on_request = [](fiy_request_t* r, const fiy::fiy_callback_t cb) {
            handle_request(static_cast<fiy::Request&>(*r), cb);
        },
        .delete_user = nullptr
    };
    return &mod_info;
}