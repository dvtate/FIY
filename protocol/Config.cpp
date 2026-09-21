#include "Config.hpp"

#include <thread>
#include <filesystem>
#include <utility>
#include <charconv>

#include "../third_party/inih/ini.h"

#include "../util/Crypto.hpp"
#include "../util/FileCache.hpp"

/**
 * Parse config file
 * @param path path to the ini config file to parse
 * @return false on error
 */
bool INIConfig::parse(const std::string& path) {
    if (!std::filesystem::exists(path)) {
        LOG("Config file " << path << " does not exist!");
        error = true;
        return false;
    }

    LOG("Loading Config file: " << path);
    error = ini_parse(
        path.c_str(),
        [](void* cfg, auto section, auto key, auto value){
            int ret = ((INIConfig*) cfg)->set_key(section, key, value);
            if (!ret) {
                LOG_ERR("Config file: invalid key: " <<key);
            }
            return ret;
        },
        this
    );
    return !error;
}

bool FiyConfig::from_file(const std::string& path) {
    const bool ret = parse(path);
    set_defaults();
    return ret;
}

bool FiyConfig::from_argv(const int argc, char** argv) {
    // TODO use a proper command line library instead
    if (argc <= 1)
        return from_file(CONFIG_FILE_PATH);

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            from_file(argv[i]);
            set_defaults();
            return false;
        }
        const char* p = argv[i];
        while (*p == '-') p++;

        if (strcmp(p, "help") == 0) {
            std::cout <<"Usage: " <<argv[0] <<" [CONFIG_FILE_PATH] [CONFIG_OVERRIDES]...\n\n"
                "\tConfig overrides have the same names as in the config file but must be prepended with '--'";
            exit(0);
            return false;
        }

        if (i + 1 >= argc) {
            LOG_ERR("Option '" << p << "': missing value");
            error = true;
            return false;
        }
        if (!set_key("", p, argv[i])) {
            error = true;
            return false;
        }
    }
    set_defaults();
    return true;
}


bool FiyConfig::set_key(const char* section, const char* key, const char* value) {
    // No section
    if (section[0] != '\0') {
        LOG_ERR("Config file: invalid section: " <<section);
        return false;
    }

    if (strcmp(key, "data_dir") == 0) {
        // Where app files are stored
        std::error_code ec;
        if (!std::filesystem::is_directory(value, ec)) {
            LOG_ERR("Config file: data_dir invalid path: " << value);
        }
        if (ec.value() != 0) {
            LOG_ERR("filesystem::is_directory failed: " << ec.message());
        }
        data_dir = value;
    } else if (strcmp(key, "hostname") == 0) {
        // Validate
        // TODO better regex that includes localhost
        const std::regex domain_name_pattern{ // supports subdomains and port numbers
            R"(^(?!-)[A-Za-z0-9-]+([\-\.]{1}[a-z0-9]+)*\.[A-Za-z]{2,6}(?:\:[0-9]{1,5})?$)"};
        if (!std::regex_match(value, domain_name_pattern)) {
            LOG_ERR("Config file: hostname '" << value << "' may be invalid (valid example: example.com)");
        }

        // Apply
        hostname = (char*) malloc(strlen(value) + 1);
        strcpy(hostname, value);
    } else if (strcmp(key, "salt") == 0) {
        salt = value;
    } else if (strcmp(key, "port") == 0) {
        int v;
        auto ec = std::from_chars(value, value + strlen(value), v).ec;
        if (ec != std::errc() || v < 0 || v > 65535) {
            LOG_ERR("Config file: port should be a valid port number, not " << value);
        } else {
            this->port = v;
        }
    } else if (strcmp(key, "concurrency") == 0) {
        char* pend = nullptr;
        int threads = strtol(value, &pend, 10); // TODO use std::from_chars
        if (pend == value) {
            LOG_ERR("Config file: concurrency should be an integer, failed to parse " << value);
            threads = 0;
        }
        if (threads <= 0) {
            concurrency = std::thread::hardware_concurrency();
            if (-threads >= concurrency)
                concurrency = 1;
            else
                concurrency += threads;
        } else {
            concurrency = threads;
        }
        if (concurrency > 50'000) {
            LOG_WARN("Concurrency level set to a massive number. Are you sure that's what you want?");
        }
    } else if (strcmp(key, "public_key") == 0) {
        public_key = load_file_as_string(value);
        if (public_key.empty()) {
            LOG_ERR("Config file: public_key: could not read file: " << value);
        }
    } else if (strcmp(key, "private_key") == 0) {
        private_key = Crypto::SSL::load_private_key_from_pem(
            load_file_as_string(value)
        );
        if (private_key == nullptr) {
            LOG_ERR("Config file: private_key: could not load file: " << value);
        }
    } else if (strcmp(key, "address") == 0) {
        listen_addr = value;
    } else if (strcmp(key, "protocol") == 0) {
        if (strcmp(value, "https") == 0 || strcmp(value, "https://") == 0)
            protocol = PROTOCOL_HTTPS;
        else if (strcmp(value, "http") == 0 || strcmp(value, "http://") == 0)
            protocol = PROTOCOL_HTTP;
        else if (strlen(value) == 0 || strcmp(value, "default") == 0)
            protocol = nullptr;
        else {
            LOG_ERR("Config file: 'protocol' should be one of:"
                    "\n\t- 'http' for internal testing"
                    "\n\t- 'https' for federation"
                    "\n\t- 'default' to let the server decide"
                " invalid value: " << value);
            protocol = nullptr;
        }
    } else {
        LOG_ERR("Invalid key: " <<key);
        return false; // invalid key
    }
    return true;
}

// Call after parse to fill in any missing items
void FiyConfig::set_defaults() {
    // Try to get keys from their default locations
    if (public_key.empty()) {
        const std::string path = data_dir + "/auth/pubkey.crt";
        public_key = load_file_as_string(path);
        if (public_key.empty()) {
            LOG_ERR("Config file: public_key: could not read file: " << path);
        }
    }
    if (private_key == nullptr) {
        const std::string path = data_dir + "/auth/privkey.pem";
        private_key = Crypto::SSL::load_private_key_from_pem(
            load_file_as_string(path)
        );
        if (private_key == nullptr) {
            LOG_ERR("Config file: private_key: could not load file: " << path);
        }
    }

    // Determine listen address if not set
    if (listen_addr.empty()) {
        const char* env_addr = std::getenv("FIY_LISTEN_ADDR");
        if (env_addr != nullptr) {
            this->listen_addr = env_addr;
        } else {
            const std::string_view hn = this->hostname;
            const bool no_port = hn.find(':') == std::string_view::npos;
            const bool localhost = hn.starts_with("127.0.0.1") || hn.starts_with("localhost");
            this->listen_addr = (no_port || localhost)
                ? "127.0.0.1"
                : "0.0.0.0";
        }
    }

    // Determine protocol
    if (protocol == nullptr) {
        protocol = (
            strchr(hostname, ':') != nullptr
            || strcmp(hostname, "localhost") == 0
            || strcmp(hostname, "127.0.0.1") == 0
        ) ? PROTOCOL_HTTP : PROTOCOL_HTTPS;
    }
}