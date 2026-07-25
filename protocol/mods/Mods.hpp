#pragma once


#include <boost/unordered/unordered_flat_map.hpp>

#include <unordered_map>

#include "Mod.hpp"
#include "ModConnectorNet.hpp"


/**
 * Interface and cache for managing the installed FIY mods/apps
 *
 * Design Considerations:
 * - Need fast lookups for mods by path+id
 * - Fast iteration over mods
 * - Server must be restarted to add/remove mods
 */
class Mods {
protected:
    std::vector<Mod*> m_mods;
    boost::unordered_flat_map<std::string, Mod*> m_mods_lookup; // path -> mod
    boost::unordered_flat_map<std::string, Mod*> m_mods_by_id; // id -> mod

    // TODO instead just look up by id (must include in requests)
    //  and then dynamic cast into ModConnectorNet to check bearer token
    //  this would eliminate need for mutex
    std::unordered_map<std::string, ModConnectorNet*> m_net_connectors; // mod http bearer tokens

    // Mods list cannot be changed while server is running
    // For now net connectors get added/removed when mod is started/stopped
    // this necessitates the mutex
    RWMutex m_mtx;
public:

    Mods() = default;
    ~Mods() {
        for (auto* m : m_mods)
                delete m;
    }

    void find_modules();
    bool start_all() const;
    bool stop_all() const;

    Mod* get_mod(const std::string& path) {
        if (const auto ret = m_mods_lookup.find(path); ret != m_mods_lookup.end())
            return ret->second;
        return nullptr;
    }

    Mod* get_mod_by_id(const std::string& id) {
        auto ret = m_mods_by_id.find(id);
        if (ret != m_mods_by_id.end())
            return ret->second;
        else
            return nullptr;
    }

    const std::vector<Mod*>& get_mods() const {
        return m_mods; // copy
    }

    /// Get JSON list of installed apps for the user portal
    [[nodiscard]] std::string get_mods_json() const {
        std::string ret = "[";
        for (auto& m : m_mods) {
            ret += m->user_json();
            ret += ',';
        }
        if (ret[ret.size() - 1] == ',')
            ret[ret.size() - 1] = ']';
        else
            ret += ']';
        return ret;
    }

    bool add_net_connector(const std::string& token, ModConnectorNet* connector) {
        RWMutex::LockForWrite lock{m_mtx};
        if (m_net_connectors.contains(token))
            return false;
        m_net_connectors[token] = connector;
        return true;
    }
    ModConnectorNet*& get_net_connector(const std::string& token) {
        RWMutex::LockForRead lock{m_mtx};
        return m_net_connectors[token];
    }
    bool has_net_connector(const std::string& token) {
        RWMutex::LockForRead lock{m_mtx};
        return m_net_connectors.contains(token);
    }
    void remove_net_connector(const std::string& token) {
        RWMutex::LockForWrite lock{m_mtx};
        m_net_connectors.erase(token);
    }

    void handle_request(
        std::shared_ptr<Session> conn,
        Mod* mod,
        const std::string& path
    );
};