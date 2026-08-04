#pragma once

#include <unordered_map>
#include <string>
#include <memory>
#include <thread>
#include <functional>

#include "../defs.hpp"
#include "../../util/RWMutex.hpp"

#include "Peer.hpp"

#include "../../modlib/fiymod.hpp"

// TODO track invalid peers so we don't waste time trying to deal with them
// TODO unordered_flat_map
// TODO std::shared_mutex
// TODO Use DB so that we don't have to reauthenticate every time
class Peers {
    RWMutex m_mtx;

    /// domain -> peer
    std::unordered_map<std::string, std::shared_ptr<Peer>> m_peers_out;

    /// bearer token -> peer
    std::unordered_map<std::string, std::shared_ptr<Peer>> m_peers_in;

    std::thread m_cron;

public:
    Peers();

//    void load_peers_from_db();

    void prune();

    bool add_peer(const std::string& domain, const std::shared_ptr<Peer>& p);
    bool remove_peer(const std::string& domain);

    std::shared_ptr<Peer> get_peer_for_domain(const std::string& domain);
    std::shared_ptr<Peer> get_peer_from_token(const std::string& token);

    std::string new_token_stub();

    void new_peer(const std::string& domain, std::function<void(std::shared_ptr<Peer>)> cb);

    void request_peer(
        const std::shared_ptr<Peer>& peer,
        const std::string& appid,
        const fiy_request_t* req
    );
    void request_peer(
        const std::string& domain,
        const std::string& appid,
        const fiy_request_t* req
    );

};
