#include <filesystem>

#include "../FIY.hpp"

#include "Mods.hpp"

void Mods::find_modules() {
    // Load mods from mods directory
    const auto mods_dir = g_fiy->config.data_dir + "/mods";
    for (auto& p : std::filesystem::directory_iterator(mods_dir))
        if (p.is_directory()) {
            const auto dir = p.path().filename().string();
            auto* m = new Mod(dir);
            if (!m->is_enabled()) {
                LOG_WARN("Mod '" <<dir <<"' is disabled.");
                delete m;
                continue;
            }
            if (!m->is_loaded()) {
                LOG_WARN("Mod '" <<dir <<"' failed to load.");
                continue;
            }
            m_mods.emplace_back(m);
        }

    // Sort alphabetically by id
    std::ranges::sort(m_mods, [](const Mod* m1, const Mod* m2) {
        return m1->id < m2->id;
    });

    // Populate lookups
    const size_t mod_count = m_mods.size();
    m_mods_lookup.reserve(mod_count);
    m_mods_by_id.reserve(mod_count);

    // Add lookup by id
    for (size_t i = 0; i < mod_count; i++)
        m_mods_by_id[m_mods[i]->id] = m_mods[i];

    // Add lookup by path (prevent overlap)
    for (size_t i = 0; i < mod_count; i++) {
        if (m_mods_lookup.contains(m_mods[i]->path)) {
            LOG("Duplicate module path " << m_mods[i]->path <<" for module " <<m_mods[i]->id << " ignored");
            const Mod* m = m_mods_lookup[m_mods[i]->path];
            LOG("Module path " << m_mods[i]->path <<" already used by module " <<m->id);
        } else {
            m_mods_lookup[m_mods[i]->path] = m_mods[i];
        }
    }

    LOG("Found " + std::to_string(m_mods.size()) + " mods.");
}

bool Mods::start_all() const {
    bool ret = true;

    for (Mod* mod: m_mods) {
        DEBUG_LOG("Starting module: " + mod->id + "...");
        if (!mod->start()) {
            LOG("Failed to start module " + mod->id <<".");
            ret = false;
        } else {
            LOG("Successfully started module " + mod->id <<".");
        }
    }
    return ret;
}


bool Mods::stop_all() const {
    bool ret = true;
    for (auto* mod : m_mods)
        if (mod->status() == Mod::Status::RUNNING)
            if (!mod->stop())
                ret = false;
    return ret;
}