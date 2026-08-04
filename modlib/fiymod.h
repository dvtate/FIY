//
// Created by tate on 7/5/24.
//

#ifndef FIY_FIYMOD_H
#define FIY_FIYMOD_H

#ifndef __cplusplus
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#else
#include <cstdint>
#include <ctime>
#include <cstdio>
#include <string>
#endif

#ifdef __cplusplus
namespace fiy {
#endif

/// String versions of http verb
extern const char* fiy_http_verb_strings[];

struct fiy_response_t;

/**
 * IPC request from a user
 */
struct fiy_request_t {
    // IDEA maybe do something like libgit2 where the actual structure is hidden
    //      and user has to do everything via functions?
    //      Then maybe we wouldn't have to copy headers

    /**
     * Relevant fediy instance
     */
    const char* domain;     // null = local user

    /**
     * User that made the request
     */
    const char* user;       // null = unauthenticated

    /**
     * Enum value corresponding to the HTTP verb sent
     */
    uint8_t method : 8;   // http method (from boost::beast::http::verb)

    /**
     * How big is the body
     * @note can't rely on null-terminators bc body could be binary format
     */
#if UINTPTR_MAX == 0xFFFFFFFF
    unsigned long long body_len : 32; // 32 bit CPU
#else
    unsigned long long body_len : 56;
#endif

    /**
     * Request path
     *
     * ie - "/post/create"
     */
    const char* path;

    /**
     * Selected headers in the user's request
     */
    const char* headers;
    /*
     * Headers we should probably include:
     * - Content-Type ?
     * - Content-Encoding ?
     * - Accept ?
     * - Accept-Encoding ?
     * - Cookie ? (without session token)
     */

    /**
     * Request body
     */
    const char* body;       // null = get request

    /**
     * Respond to this request
     * @remarks the request pointer should be the request being responded to (ie - this)
     * @remarks the response should be copied or used before returning to event loop
     */
    void (*callback)(const struct fiy_request_t*, const struct fiy_response_t*);

    /**
     * Possibly used by the respond callback
     */
    const void* context;
};

/**
 * Buffered reader
 * @param context (in) user-defined context used to determine content to read
 * @param buffer (out) buffer to hold data that's being read
 * @param max_bytes (in) maximum number of bytes to accept
 * @return number of bytes read, returns zero when done reading
 * @remark when done reading, do cleanup on context object then return zero
 */
typedef ssize_t (*fiy_buffered_reader_fn)(
    void* context,
    char* buffer,
    size_t max_bytes
);

/**
 * Enum for body tagged union
 */
typedef enum {
    FIY_BODY_NONE,
    FIY_BODY_BUFFER,
    FIY_BODY_FILE,
    FIY_BODY_READER
} fiy_body_type;

/**
 * Various ways to pass the body of a response
 *
 * For now this is only used for responses
 */
struct fiy_body_t {
    fiy_body_type type;

    union {
        /// String/buffer
        struct {
            const char* data;
            size_t length;
        } buffer;

        /// File
        // TODO instead int fd + int offset
        struct {
            int fd;
            long offset;
        } file;

        /// Buffered reader
        struct {
            fiy_buffered_reader_fn read;
            void* context;
        } reader;
    };
};

/**
 * Response to a request
 */
struct fiy_response_t {
    /**
     * HTTP Status code
     */
    int status;

    /**
     * HTTP Headers to set
     */
    const char* headers;

    /**
     * HTTP Body
     */
    struct fiy_body_t body;
};

/// This is used to provide callbacks to the host
struct fiy_mod_info_t {
    /// Handle http requests to the module
    void (*on_request)(const struct fiy_request_t* request)
#ifdef __cplusplus
        {nullptr}
#endif
    ;

    /// Called by host to delete all data associated with a deleted user
    /// @note username of form user@domain
    void (*delete_user)(const char* username)
#ifdef __cplusplus
        {nullptr}
#endif
    ;

    ////
    // These can be overridden by user's module.json
    ////

    /**
     * Globally unique mod id
     */
    const char* id
#ifdef __cplusplus
        {nullptr}
#endif
    ;

    /**
     * Version
     */
    const char* version
#ifdef __cplusplus
        {nullptr}
#endif
    ;
};

/**
 * Basic information about a user
 */
struct fiy_local_user_info_t {
    /// True if user is administrator
    bool admin;

    /// Unix timestamp when the user created their account
    int64_t join_ts;
};

/// Some info from the host that may be relevant to the module
struct fiy_host_info_t {
    /**
     * Fediy instance domain
     */
    const char* domain;

    /**
     * unique app id for this app
     */
    const char* app_id;

    /**
     * Base uri for this app
     * - format: <protocol>://<host>/<path>
     * - ie - https://bodge.dev/git
     */
    const char* base_uri;

    /**
     * Where this app's files go
     */
    const char* data_dir;

    /**
     * Write a debug message
     * @param level
     *  - 0 : fatal
     *  - 1 : error
     *  - 2 : warning
     *  - 3 : info
     *  - 4 : debug
     * @param message
     */
    // TODO should take host_info self pointer so that it's handled correctly
    void (*log)(int level, const char* message);

    /**
     * Send a request to another app
     * @param app_id app to send the request to
     * @param request request to send to the other app
     *      method   - http method
     *      path     - uri path
     *      domain   - remote server to send request to or nullptr if local inter-app request
     *      user     - local user or nullptr if unauthenticated
     *                  cannot be remote user, user's host is only instance that can act on their behalf
     *      callback - called by other mod to provide response
     *      context  - data relevant to handle callback
     * @notes
     * - local apps can send requests to each other without restrictions
     * - an app on server a can only send requests to apps on server b on behalf of users residing on server a
     *    - this prevents false impersonation
     */
    void (*request)(const char* app_id, const struct fiy_request_t* request);

    /**
     * Authenticate an instance-local user
     * @param username user's username
     * @param password user's password
     * @return
     *      0 if valid credentials
     *      1 invalid credentials
     *      2 user locked
     *      -1 error
     */
    int (*local_login)(const char* username, const char* password);

    /**
     * Get information for a given local user
     * @param local_user_name username for relevant user
     * @param ret struct to put the results into
     * @return
     *  0 on success
     *  1 if the user does not exist
     *  -1 error
     */
    int (*user_info)(const char* local_user_name, struct fiy_local_user_info_t* ret);

    /**
     * Gets the current time according to the host
     */
    time_t (*now)();

    /**
     * Path to module.json file
     */
    const char* mod_config;

    // TODO this could be used to provide context to other functions
    // struct Mod;
    // Mod* mod;

    // TODO get a list of installed mods (by id)
};

typedef struct fiy_mod_info_t* (*fiy_mod_start_function_t)(const struct fiy_host_info_t*);

#ifdef __cplusplus
} // namespace fiy
#endif

#ifdef __cplusplus
#define FIY_EXPORT  extern "C" __attribute__((visibility("default")))
#else
#define FIY_EXPORT  __attribute__((visibility("default")))
#endif
#endif //FIY_FIYMOD_H
