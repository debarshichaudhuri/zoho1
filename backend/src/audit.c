#include "../include/crypto.h"

// ============================================================
// SHA-256 Chain Audit Trail
//
// Every financial action is logged with a SHA-256 hash that
// chains to the previous entry's hash. Tampering with any
// historical record breaks the chain — instantly detectable.
//
// Hash = SHA256(prev_hash + action + entity_type + entity_id + details + timestamp)
// ============================================================

// Get the most recent hash from the audit trail
static bool audit_get_last_hash(AppState *app, char last_hash[65]) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT hash FROM audit_trail ORDER BY id DESC LIMIT 1;";

    if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        strcpy(last_hash, "GENESIS");
        return false;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *h = (const char*)sqlite3_column_text(stmt, 0);
        if (h) {
            strncpy(last_hash, h, 64);
            last_hash[64] = '\0';
        } else {
            strcpy(last_hash, "GENESIS");
        }
    } else {
        strcpy(last_hash, "GENESIS");
    }

    sqlite3_finalize(stmt);
    return true;
}

// ============================================================
// Log an auditable action with SHA-256 chain hash
// ============================================================
bool audit_log_action(AppState *app, const char *action, const char *entity_type,
                      int64_t entity_id, const char *details) {
    if (!app || !app->db || !action) return false;

    // 1. Get previous hash
    char prev_hash[65];
    audit_get_last_hash(app, prev_hash);

    // 2. Build the data string to hash
    //    Format: "prev_hash|action|entity_type|entity_id|details|timestamp"
    int64_t timestamp = (int64_t)time(NULL);
    char hash_input[2048];
    snprintf(hash_input, sizeof(hash_input), "%s|%s|%s|%ld|%s|%ld",
             prev_hash,
             action,
             entity_type ? entity_type : "",
             (long long)entity_id,
             details ? details : "",
             (long long)timestamp);

    // 3. Compute SHA-256
    unsigned char hash_bytes[32];
    crypto_sha256((const unsigned char*)hash_input, strlen(hash_input), hash_bytes);

    char hash_hex[65];
    crypto_hash_to_hex(hash_bytes, hash_hex);

    // 4. Insert into audit_trail
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO audit_trail "
                      "(action, entity_type, entity_id, details, hash, prev_hash, created_at) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?);";

    if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[AUDIT] Failed to prepare: %s\n", sqlite3_errmsg(app->db));
        return false;
    }

    sqlite3_bind_text(stmt, 1, action, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, entity_type ? entity_type : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, entity_id);
    sqlite3_bind_text(stmt, 4, details ? details : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, hash_hex, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, prev_hash, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 7, timestamp);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);

    if (ok) {
        printf("[AUDIT] %s %s #%ld — hash: %.16s...\n",
               action, entity_type ? entity_type : "", (long long)entity_id, hash_hex);
    }

    return ok;
}

// ============================================================
// Verify audit chain integrity
// Returns: number of broken links (0 = fully valid chain)
// ============================================================
int audit_verify_chain(AppState *app) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, action, entity_type, entity_id, details, "
                      "hash, prev_hash, created_at "
                      "FROM audit_trail ORDER BY id ASC;";

    if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    int broken = 0;
    int total = 0;
    char expected_prev[65] = "GENESIS";

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        total++;
        int64_t id = sqlite3_column_int64(stmt, 0);
        const char *action = (const char*)sqlite3_column_text(stmt, 1);
        const char *entity_type = (const char*)sqlite3_column_text(stmt, 2);
        int64_t entity_id = sqlite3_column_int64(stmt, 3);
        const char *details = (const char*)sqlite3_column_text(stmt, 4);
        const char *stored_hash = (const char*)sqlite3_column_text(stmt, 5);
        const char *stored_prev = (const char*)sqlite3_column_text(stmt, 6);
        int64_t timestamp = sqlite3_column_int64(stmt, 7);

        // Check 1: prev_hash should match expected
        if (strcmp(stored_prev ? stored_prev : "", expected_prev) != 0) {
            fprintf(stderr, "[AUDIT] CHAIN BREAK at #%ld: prev_hash mismatch\n", (long long)id);
            broken++;
        }

        // Check 2: Recompute hash and verify
        char hash_input[2048];
        snprintf(hash_input, sizeof(hash_input), "%s|%s|%s|%ld|%s|%ld",
                 stored_prev ? stored_prev : "GENESIS",
                 action ? action : "",
                 entity_type ? entity_type : "",
                 (long long)entity_id,
                 details ? details : "",
                 (long long)timestamp);

        unsigned char recomputed[32];
        crypto_sha256((const unsigned char*)hash_input, strlen(hash_input), recomputed);

        char recomputed_hex[65];
        crypto_hash_to_hex(recomputed, recomputed_hex);

        if (stored_hash && strcmp(stored_hash, recomputed_hex) != 0) {
            fprintf(stderr, "[AUDIT] HASH TAMPERED at #%ld: stored=%s computed=%s\n",
                    (long)id, stored_hash, recomputed_hex);
            broken++;
        }

        // Update expected prev for next iteration
        if (stored_hash) {
            strncpy(expected_prev, stored_hash, 64);
            expected_prev[64] = '\0';
        }
    }

    sqlite3_finalize(stmt);

    if (broken == 0) {
        printf("[AUDIT] Chain verified: %d entries, FULLY INTACT ✓\n", total);
    } else {
        fprintf(stderr, "[AUDIT] Chain verified: %d entries, %d BROKEN LINKS ✗\n", total, broken);
    }

    return broken;
}
