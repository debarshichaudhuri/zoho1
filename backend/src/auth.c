#include "../include/qmanage.h"
#include "../include/crypto.h"
#include <time.h>

// ============================================================
// Session Token Authentication
// Token format: 64-char hex (32 random bytes)
// Storage: sessions table with 24-hour expiry
// Header: X-Auth-Token: <token>
// ============================================================

#define SESSION_TTL_SECONDS  (24 * 3600)  // 24 hours
#define TOKEN_HEX_LEN        64

// Generate a cryptographically random 32-byte token (64 hex chars)
static void generate_token(char out[TOKEN_HEX_LEN + 1]) {
    unsigned char rand_bytes[32];
    mg_random(rand_bytes, 32);
    crypto_hash_to_hex(rand_bytes, out); // reuse hex formatter (same 64-char output)
}

// Hash a password with its salt using PBKDF2-SHA256
static void hash_password(const char *password, const char *salt_hex, char hash_hex[65]) {
    // Decode hex salt to bytes
    unsigned char salt[16];
    for (int i = 0; i < 16; i++) {
        unsigned int b;
        sscanf(salt_hex + (i * 2), "%02x", &b);
        salt[i] = (unsigned char)b;
    }

    unsigned char key[32];
    crypto_derive_key(password, salt, 100000, key);
    crypto_hash_to_hex(key, hash_hex);
}

// ============================================================
// POST /api/auth/login
// Body: {"username": "admin", "password": "..."}
// Response: {"token": "...", "expires_in": 86400, "role": "admin"}
// ============================================================
void route_auth_login(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    if (mg_vcasecmp(&hm->method, "POST") != 0) {
        send_error(c, 405, "Method not allowed");
        return;
    }

    cJSON *body = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
    if (!body) { send_error(c, 400, "Invalid JSON"); return; }

    cJSON *username_j = cJSON_GetObjectItem(body, "username");
    cJSON *password_j = cJSON_GetObjectItem(body, "password");

    if (!username_j || !cJSON_IsString(username_j) ||
        !password_j || !cJSON_IsString(password_j)) {
        cJSON_Delete(body);
        send_error(c, 400, "username and password required");
        return;
    }

    // Copy strings before freeing body (password_j->valuestring would dangle after cJSON_Delete)
    char username_copy[128], password_copy[256];
    strncpy(username_copy, username_j->valuestring, sizeof(username_copy) - 1);
    username_copy[sizeof(username_copy) - 1] = '\0';
    strncpy(password_copy, password_j->valuestring, sizeof(password_copy) - 1);
    password_copy[sizeof(password_copy) - 1] = '\0';

    // Look up user
    DbParam p[] = { DB_P_TEXT(username_copy) };
    cJSON *users = db_query_typed(app,
        "SELECT id, username, password_hash, password_salt, role FROM users "
        "WHERE username = ? AND is_active = 1 LIMIT 1;",
        p, 1);

    cJSON_Delete(body);

    if (!users || cJSON_GetArraySize(users) == 0) {
        cJSON_Delete(users);
        send_error(c, 401, "Invalid credentials");
        return;
    }

    cJSON *user = cJSON_GetArrayItem(users, 0);
    // Copy all needed fields from users before freeing it
    char role_copy[32] = "admin";
    char hash_copy[65] = {0};
    char salt_copy[33] = {0};
    int  user_id = 0;
    const char *tmp_hash = cJSON_GetStringValue(cJSON_GetObjectItem(user, "password_hash"));
    const char *tmp_salt = cJSON_GetStringValue(cJSON_GetObjectItem(user, "password_salt"));
    const char *tmp_role = cJSON_GetStringValue(cJSON_GetObjectItem(user, "role"));
    cJSON *tmp_id = cJSON_GetObjectItem(user, "id");
    if (tmp_hash) { strncpy(hash_copy, tmp_hash, 64); hash_copy[64] = '\0'; }
    if (tmp_salt) { strncpy(salt_copy, tmp_salt, 32); salt_copy[32] = '\0'; }
    if (tmp_role) { strncpy(role_copy, tmp_role, 31); role_copy[31] = '\0'; }
    if (tmp_id)   { user_id = (int)tmp_id->valuedouble; }
    cJSON_Delete(users);

    // Hash the submitted password with stored salt
    char computed_hash[65];
    hash_password(password_copy, salt_copy, computed_hash);

    if (strcmp(computed_hash, hash_copy) != 0) {
        send_error(c, 401, "Invalid credentials");
        return;
    }

    // Generate session token
    char token[TOKEN_HEX_LEN + 1];
    generate_token(token);

    int64_t now = (int64_t)time(NULL);
    int64_t expires_at = now + SESSION_TTL_SECONDS;

    // Store session
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO sessions (token, user_id, expires_at, created_at) "
                      "VALUES (?, ?, ?, ?);";
    if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, token, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, user_id);
        sqlite3_bind_int64(stmt, 3, expires_at);
        sqlite3_bind_int64(stmt, 4, now);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // Update last_login
    sqlite3_stmt *upd;
    if (sqlite3_prepare_v2(app->db, "UPDATE users SET last_login = ? WHERE id = ?;",
                           -1, &upd, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(upd, 1, now);
        sqlite3_bind_int(upd, 2, user_id);
        sqlite3_step(upd);
        sqlite3_finalize(upd);
    }

    // Clean up expired sessions in background
    db_execute(app, "DELETE FROM sessions WHERE expires_at < strftime('%s','now');");

    cJSON *res = cJSON_CreateObject();
    cJSON_AddStringToObject(res, "token", token);
    cJSON_AddNumberToObject(res, "expires_in", SESSION_TTL_SECONDS);
    cJSON_AddStringToObject(res, "role", role_copy);
    send_json(c, 200, res);
}

// ============================================================
// POST /api/auth/logout
// ============================================================
void route_auth_logout(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    struct mg_str *token_hdr = mg_http_get_header(hm, "X-Auth-Token");
    if (token_hdr && token_hdr->len > 0) {
        char token[TOKEN_HEX_LEN + 2];
        size_t tlen = token_hdr->len < TOKEN_HEX_LEN ? token_hdr->len : TOKEN_HEX_LEN;
        memcpy(token, token_hdr->buf, tlen);
        token[tlen] = '\0';

        DbParam p[] = { DB_P_TEXT(token) };
        db_execute_typed(app, "DELETE FROM sessions WHERE token = ?;", p, 1);
    }

    cJSON *res = cJSON_CreateObject();
    cJSON_AddStringToObject(res, "status", "logged_out");
    send_json(c, 200, res);
}

// ============================================================
// POST /api/auth/change-password
// Body: {"current_password": "...", "new_password": "..."}
// ============================================================
void route_auth_change_password(struct mg_connection *c, struct mg_http_message *hm,
                                 AppState *app, int user_id) {
    cJSON *body = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
    if (!body) { send_error(c, 400, "Invalid JSON"); return; }

    cJSON *cur_j = cJSON_GetObjectItem(body, "current_password");
    cJSON *new_j = cJSON_GetObjectItem(body, "new_password");

    if (!cur_j || !cJSON_IsString(cur_j) || !new_j || !cJSON_IsString(new_j)) {
        cJSON_Delete(body);
        send_error(c, 400, "current_password and new_password required");
        return;
    }

    // Verify current password
    DbParam p[] = { DB_P_INT(user_id) };
    cJSON *users = db_query_typed(app,
        "SELECT password_hash, password_salt FROM users WHERE id = ?;", p, 1);

    if (!users || cJSON_GetArraySize(users) == 0) {
        cJSON_Delete(body); cJSON_Delete(users);
        send_error(c, 404, "User not found");
        return;
    }

    cJSON *user = cJSON_GetArrayItem(users, 0);
    const char *stored_hash = cJSON_GetStringValue(cJSON_GetObjectItem(user, "password_hash"));
    const char *stored_salt = cJSON_GetStringValue(cJSON_GetObjectItem(user, "password_salt"));

    char computed[65];
    hash_password(cur_j->valuestring, stored_salt, computed);

    if (strcmp(computed, stored_hash) != 0) {
        cJSON_Delete(body); cJSON_Delete(users);
        send_error(c, 401, "Current password is incorrect");
        return;
    }
    cJSON_Delete(users);

    // Generate new salt and hash
    unsigned char new_salt_bytes[16];
    mg_random(new_salt_bytes, 16);
    char new_salt_hex[33];
    for (int i = 0; i < 16; i++) sprintf(new_salt_hex + i*2, "%02x", new_salt_bytes[i]);
    new_salt_hex[32] = '\0';

    char new_hash[65];
    hash_password(new_j->valuestring, new_salt_hex, new_hash);
    cJSON_Delete(body);

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(app->db,
            "UPDATE users SET password_hash = ?, password_salt = ? WHERE id = ?;",
            -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, new_hash, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, new_salt_hex, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, user_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // Invalidate all other sessions for this user
    DbParam dp[] = { DB_P_INT(user_id) };
    db_execute_typed(app, "DELETE FROM sessions WHERE user_id = ?;", dp, 1);

    cJSON *res = cJSON_CreateObject();
    cJSON_AddStringToObject(res, "status", "password_changed");
    send_json(c, 200, res);
}

// ============================================================
// Middleware: validate X-Auth-Token header
// Returns user_id > 0 on success, 0 if unauthorized
// Sets HTTP 401 response on failure
// ============================================================
int auth_validate(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    struct mg_str *token_hdr = mg_http_get_header(hm, "X-Auth-Token");
    if (!token_hdr || token_hdr->len == 0) {
        send_error(c, 401, "Authentication required — provide X-Auth-Token header");
        return 0;
    }

    char token[TOKEN_HEX_LEN + 2];
    size_t tlen = token_hdr->len < TOKEN_HEX_LEN ? token_hdr->len : TOKEN_HEX_LEN;
    memcpy(token, token_hdr->buf, tlen);
    token[tlen] = '\0';

    DbParam p[] = { DB_P_TEXT(token) };
    cJSON *rows = db_query_typed(app,
        "SELECT s.user_id FROM sessions s "
        "WHERE s.token = ? AND s.expires_at > strftime('%s','now') LIMIT 1;",
        p, 1);

    int user_id = 0;
    if (rows && cJSON_GetArraySize(rows) > 0) {
        cJSON *row = cJSON_GetArrayItem(rows, 0);
        cJSON *uid = cJSON_GetObjectItem(row, "user_id");
        if (uid) user_id = (int)uid->valuedouble;
    }
    cJSON_Delete(rows);

    if (user_id == 0) {
        send_error(c, 401, "Invalid or expired token — please login again");
    }
    return user_id;
}
