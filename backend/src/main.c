#include "../include/qmanage.h"

AppState app_state;

// Mongoose Event Handler
static void ev_handler(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;
        AppState *app = (AppState *) c->fn_data;

        printf("[REQUEST] %.*s %.*s\n",
               (int)hm->method.len, hm->method.buf,
               (int)hm->uri.len,    hm->uri.buf);

        // Handle CORS preflight (OPTIONS) for all /api/* routes
        // Browsers send this before every cross-origin POST/PUT/DELETE
        if (mg_vcasecmp(&hm->method, "OPTIONS") == 0) {
            mg_http_reply(c, 204,
                "Access-Control-Allow-Origin: *\r\n"
                "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
                "Access-Control-Allow-Headers: Content-Type, X-Auth-Token\r\n"
                "Access-Control-Max-Age: 86400\r\n",
                "");
            return;
        }

        // Route API calls
        if (hm->uri.len >= 4 && strncmp(hm->uri.buf, "/api", 4) == 0) {
            handle_api_request(c, hm, app);
        } else {
            // Serve static frontend files
            struct mg_http_serve_opts opts = {0};
            opts.root_dir = "C:/testtproj";
            mg_http_serve_dir(c, hm, &opts);
        }
    }
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0); // Disable buffering
    printf("Initializing Q Manage C Backend (Ultra-Fast Edition)...\n");

    // Initialize Database
    if (!db_init(&app_state)) {
        fprintf(stderr, "Failed to initialize database. Exiting.\n");
        return 1;
    }
    printf("SQLite database initialized at %s (WAL Mode enabled)\n", DB_PATH);

    // Initialize Mongoose Event Manager
    mg_mgr_init(&app_state.mgr);
    
    // Create HTTP Listener
    char listen_url[32];
    snprintf(listen_url, sizeof(listen_url), "http://0.0.0.0:%s", PORT);
    
    if (mg_http_listen(&app_state.mgr, listen_url, ev_handler, &app_state) == NULL) {
        fprintf(stderr, "Cannot listen on %s\n", listen_url);
        return 1;
    }

    printf("Server listening on %s\n", listen_url);
    printf("Press Ctrl+C to exit.\n");

    // Infinite Event Loop
    for (;;) {
        mg_mgr_poll(&app_state.mgr, 1000); // 1-second timeout
    }

    // Cleanup
    mg_mgr_free(&app_state.mgr);
    db_close(&app_state);

    return 0;
}
