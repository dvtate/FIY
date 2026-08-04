#include <string.h>
#include <stdlib.h>

#include "fiymod.h"

struct fiy_host_info_t* g_host_info;

/// Handle incoming requests to for this mod
static void handle_request(const struct fiy_request_t* request) {
    // Allocate a body string to send to the user
    const size_t body_len = 50
        + (request->user != NULL ? strlen(request->user) : 4)
        + (request->domain != NULL ? strlen(request->domain) : 4)
        + (10)
        + (request->path != NULL ? strlen(request->path) : 4);
    char* body = malloc(sizeof(char) * body_len);

    // Construct body string
    snprintf(
        body,
        body_len,
        "Hello, @%s@%s! <br/>Action: %s %s",
        request->user == NULL ? "null" : request->user,
        request->domain == NULL ? "null" : request->domain,
        fiy_http_verb_strings[request->method],
        request->path == NULL ? "null" : request->path
    );

    // Pass response to callback
    const struct fiy_response_t resp = {
        .status = 200,
        .headers = "Content-Type: text/html",
        .body = {
            .type = FIY_BODY_BUFFER,
            .buffer = { .data = body, .length = strlen(body) }
        }
    };
    request->callback(request, &resp);

    // Cleanup
    free(body);
}

/// Handle user deletion
static void user_deleted(const char* username) {
    printf("User %s deleted, removing their data (ie - for GDPR compliance)", username);
}

/// Export that gets called before anything else
FIY_EXPORT struct fiy_mod_info_t* start(const struct fiy_host_info_t* host_info) {
    // Prepare and make sure everything is set up and installed correctly
    static struct fiy_mod_info_t mod_info = {
        .on_request = handle_request,
        .delete_user = user_deleted,
        .id = "example.c",
        .version = "0.0"
    };
    return &mod_info;
}

/// Export that gets called for the mod to do any cleanup before exit/dlclose
FIY_EXPORT void stop() {
    printf("Stopping mod");
}
