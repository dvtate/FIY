//
// Created by tate on 7/8/25.
//

#include "LocalUsers.hpp"

#include <utility>

#include "../FIY.hpp"
#include "../DB.hpp"

using DB::operator ""_sql;

LocalUsers::LocalUsers() {
    m_cron = std::thread([this]() {
        while (true) {
            std::this_thread::sleep_for(
                    std::chrono::seconds(60 * 5));
            this->cron();
        }
    });
}

/**
 * Add a new user to the database
 * @param user
 * @param password
 * @return true if user was added, false otherwise
 */
bool LocalUsers::add_user(const LocalUser& user, std::string password) {
    thread_local auto q = "INSERT INTO Users"
        " (username, isAdmin, hashedPassword, email, joinTs) "
        "VALUES (?, ?, ?, ?, ?)"_sql;

    unsigned char buff[LocalUser::PASSWORD_HASH_SIZE];
    unsigned char* hp = LocalUser::hash_password(std::move(password), buff);

    q.bindNoCopy(1, user.get_username());
    q.bind(2, (int32_t)user.is_admin);
    q.bindNoCopy(3, (void*)hp, LocalUser::PASSWORD_HASH_SIZE);
    q.bindNoCopy(4, user.email);
    q.bind(5, (int64_t) user.joined_ts);
    const auto ret = q.exec();
    q.clearBindings();
    q.reset();
    return ret > 0;

    // Add user to cache
    RWMutex::LockForWrite lock{m_mtx};
    m_username_cache[user.get_username()] = std::make_shared<LocalUser>(user);
}

/**
 * Get user info for given username
 * @param username user to find
 * @return LocalUser object or null if no user exists with given username
 */
std::shared_ptr<LocalUser> LocalUsers::get_user(const std::string& username) {
    // Fetch user from cache
    {
        RWMutex::LockForRead lock{m_mtx};
        auto it = m_username_cache.find(username);
        if (it != m_username_cache.end())
            return it->second;
    }

    // Fetch user from database
    thread_local auto query = "SELECT isAdmin, email, joinTs"
        " FROM Users WHERE username = ?"_sql;
    query.bindNoCopy(1, username);
    if (!query.executeStep()) {
        query.reset();
        return nullptr; // user not in the database
    }
    auto user = std::make_shared<LocalUser>(
        username,
        query.getColumn(0).getInt() != 0, // isAdmin
        query.getColumn(1).getString(),  // email
        query.getColumn(2).getUInt()  // join_ts
    );
    query.reset();

    // Update cache
    {
        RWMutex::LockForWrite lock{m_mtx};
        m_username_cache[username] = user;
        return user;
    }
}

/**
 * Validate user login credentials
 * @param username local user username
 * @param password local user password
 * @return 0 on success, 1 on login fail, -1 on error
 */
int LocalUsers::auth_user(const std::string& username, std::string password) {
    // Get query
    thread_local auto q = "SELECT hashedPassword FROM Users WHERE username = ?"_sql;
    try {
        q.bindNoCopy(1, username);
        if (!q.executeStep()) {
            q.reset();
            return 0;
        }
        const auto ret = LocalUser::check_password_hash(
            std::move(password),
            q.getColumn(0)
        );
        q.reset();
        return ret ? 0 : 1;
    } catch (const SQLite::Exception& e) {
        DEBUG_LOG("DB Error: " << e.what());
        return -1;
    }
}

/**
 * Authenticate user auth token
 * @param auth_token User-provided authentication token
 * @return user object or null if invalid token
 */
std::shared_ptr<LocalUser> LocalUsers::auth_user(const std::string& auth_token) {
    RWMutex::LockForRead lock{m_mtx};

    const auto it = m_token_cache.find({nullptr, auth_token, 0});
    if (it == m_token_cache.end())
        return nullptr;

    // Cron should remove tokens when they become invalid
//        if (it->is_expired())
//            return nullptr;

    return it->m_user;
}

/**
 * Run periodically to remove expired auth tokens
 */
void LocalUsers::cron() {
    RWMutex::LockForWrite lock{m_mtx};

    // Remove expired auth tokens
    std::erase_if(m_token_cache,
        [now = g_fiy->now()] (const LocalUser::AuthToken& t) {
            return t.is_expired(now);
        }
    );
}

/**
 * De-authenticate a token (ie - to sign a user out)
 * @param auth_token token to deauthenticate
 */
void LocalUsers::deauth_token(const std::string& auth_token) {
    const LocalUser::AuthToken token{nullptr, auth_token, 0};
    RWMutex::LockForWrite lock{m_mtx};
    m_token_cache.erase(token);
}

/**
 * Create an auth token for the user
 * @param user User to authorize
 * @return Auth token for the user
 */
LocalUser::AuthToken LocalUsers::authorize_user(std::shared_ptr<LocalUser> user) {
    // Make new auth token for them
    RWMutex::LockForWrite lock{m_mtx};
    LocalUser::AuthToken token{std::move(user)};
    while (m_token_cache.contains(token))
        token.m_token = LocalUser::AuthToken::get_token_string();
    m_token_cache.insert(token);
    return token;
}

/**
 * Log a user in, providing a valid authentication token on success
 */
LocalUser::AuthToken LocalUsers::login_user(const std::string& username, std::string password) {
    // TODO could better utilize cache and always do a single DB query

    // Check username
    std::shared_ptr<LocalUser> user = get_user(username);
    if (user == nullptr)
        return {};

    // Check password
    if (auth_user(username, std::move(password)) != 0)
        return {};

    // Make authorization token
    return authorize_user(std::move(user));
}

/**
 * Delete a user
 */
void LocalUsers::delete_user(const std::string& username) {
    const auto user = get_user(username);
    if (user == nullptr)
        return;

    RWMutex::LockForWrite lock{m_mtx};

    // Log out user & remove them from cache
    std::erase_if(m_token_cache, [&](const LocalUser::AuthToken& t) {
        return t.m_user == user || t.m_user->get_username() == username;
    });

    // Inform mods
    for (Mod* m : g_fiy->mods.get_mods())
        m->ipc->delete_user(username.c_str());

    // Remove from username cache
    m_username_cache.erase(username);

    // Remove user from database
    thread_local auto q = "DELETE FROM Users WHERE username = ?"_sql;
    q.bindNoCopy(1, username);
    q.exec();
    q.reset();
}