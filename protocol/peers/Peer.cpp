//
// Created by tate on 7/31/25.
//

#include  "../util/FileCache.hpp"

#include "../FIY.hpp"

#include "Peer.hpp"


std::string Peer::sig(
    const std::string& appid,
    const std::string& path,
    const std::string& user,
    const std::size_t body_len,
    const std::string& ts_str = std::to_string(g_fiy->now())
) const {
    return Crypto::hmac(auth.sym_key,
        concat("FIY", appid , path , user , std::to_string(body_len), ts_str)
    );
}

std::shared_ptr<Peer> Peer::parse_handshake_request(std::string_view body, std::string& signature) {
    // TODO encryption

    // Get body components
    // Body should contain the following:
    //  - peer domain
    //  - part of a secret
    //  - bearer token
    //  - signature
    std::string_view domain, secret, bearer_token;
    auto eol = body.find('\n');
    if (eol != std::string_view::npos) {
        domain = body.substr(0, eol);
        size_t start = eol + 1;
        eol = body.find('\n', start);
        if (eol != std::string_view::npos) {
            secret = body.substr(start, eol - start);
            start = eol + 1;
            eol = body.find('\n', start);
            if (eol != std::string_view::npos) {
                bearer_token = body.substr(start, eol - start);
                signature = body.substr(eol + 1);
            }
        }
    }

    // Missing components
    if (signature.empty() || bearer_token.empty() || secret.empty() || domain.empty()) {
        return nullptr;
    }

    return std::make_shared<Peer>(domain, PeerAuth(
        std::string(secret),
        std::string(bearer_token)));
}
