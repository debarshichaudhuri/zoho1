#include "../include/qmanage.h"
#include "../include/crypto.h"
#include <stdio.h>
#include <time.h>

// ============================================================
// AES-256-GCM Encrypted Backup Engine
// Key derivation: PBKDF2-SHA256 (100,000 iterations)
// Encryption: AES-256-GCM (authenticated, tamper-proof)
// File format: [12-byte magic | 16-byte salt | AES-GCM payload]
// ============================================================

#define BACKUP_MAGIC      "QBAK_AES_V2\x00"
#define BACKUP_MAGIC_LEN  12
#define BACKUP_SALT_LEN   16

static bool write_encrypted_backup(const char *filepath, const unsigned char *data,
                                   size_t data_len, const char *passphrase) {
    // 1. Generate random salt for PBKDF2
    unsigned char salt[BACKUP_SALT_LEN];
    mg_random(salt, BACKUP_SALT_LEN);

    // 2. Derive AES-256 key from passphrase
    unsigned char key[32];
    crypto_derive_key(passphrase, salt, 100000, key);

    // 3. Encrypt with AES-256-GCM
    size_t enc_len = 0;
    unsigned char *encrypted = crypto_aes256gcm_encrypt(data, data_len, key, &enc_len);
    memset(key, 0, sizeof(key)); // zero key from memory immediately

    if (!encrypted) {
        fprintf(stderr, "Backup: AES-256-GCM encryption failed\n");
        return false;
    }

    // 4. Write: [magic][salt][encrypted_payload]
    FILE *f = fopen(filepath, "wb");
    if (!f) {
        free(encrypted);
        fprintf(stderr, "Backup: Cannot write encrypted file %s\n", filepath);
        return false;
    }

    fwrite(BACKUP_MAGIC, 1, BACKUP_MAGIC_LEN, f);
    fwrite(salt, 1, BACKUP_SALT_LEN, f);
    fwrite(encrypted, 1, enc_len, f);
    fclose(f);
    free(encrypted);

    printf("[BACKUP] AES-256-GCM encrypted: %lu bytes -> %lu bytes\n",
           (unsigned long)data_len, (unsigned long)(BACKUP_MAGIC_LEN + BACKUP_SALT_LEN + enc_len));
    return true;
}

static unsigned char* read_encrypted_backup(const char *filepath, const char *passphrase,
                                             size_t *out_len) {
    FILE *f = fopen(filepath, "rb");
    if (!f) return NULL;

    // Read and verify magic
    char magic[BACKUP_MAGIC_LEN];
    if (fread(magic, 1, BACKUP_MAGIC_LEN, f) != BACKUP_MAGIC_LEN ||
        memcmp(magic, BACKUP_MAGIC, BACKUP_MAGIC_LEN) != 0) {
        fclose(f);
        fprintf(stderr, "Restore: Invalid or unsupported backup format\n");
        return NULL;
    }

    // Read salt
    unsigned char salt[BACKUP_SALT_LEN];
    if (fread(salt, 1, BACKUP_SALT_LEN, f) != BACKUP_SALT_LEN) {
        fclose(f);
        return NULL;
    }

    // Read encrypted payload
    fseek(f, 0, SEEK_END);
    long total = ftell(f);
    long payload_offset = BACKUP_MAGIC_LEN + BACKUP_SALT_LEN;
    long payload_len = total - payload_offset;
    fseek(f, payload_offset, SEEK_SET);

    if (payload_len <= 0) {
        fclose(f);
        return NULL;
    }

    unsigned char *enc_buf = malloc(payload_len);
    if (!enc_buf) { fclose(f); return NULL; }
    fread(enc_buf, 1, payload_len, f);
    fclose(f);

    // Derive key from passphrase + stored salt
    unsigned char key[32];
    crypto_derive_key(passphrase, salt, 100000, key);

    // Decrypt — AES-256-GCM will reject tampered data
    unsigned char *plaintext = crypto_aes256gcm_decrypt(enc_buf, payload_len, key, out_len);
    memset(key, 0, sizeof(key));
    free(enc_buf);

    if (!plaintext) {
        fprintf(stderr, "Restore: AES-256-GCM authentication FAILED — wrong passphrase or tampered file!\n");
    }
    return plaintext;
}

// ============================================================
// Create a full database backup
// ============================================================
char* backup_create(AppState *app, const char *backup_dir, const char *passphrase) {
    if (!app || !app->db) return NULL;

    // 1. Generate filename with timestamp
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    static char filepath[512];
    snprintf(filepath, sizeof(filepath),
        "%s/qmanage_backup_%04d%02d%02d_%02d%02d%02d.qbak",
        backup_dir,
        t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
        t->tm_hour, t->tm_min, t->tm_sec);

    // 2. Use SQLite Online Backup API for crash-safe copy to temp file
    char tmp_path[560];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", filepath);

    sqlite3 *backup_db;
    int rc = sqlite3_open(tmp_path, &backup_db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Backup: Cannot create temp file: %s\n", sqlite3_errmsg(backup_db));
        return NULL;
    }

    sqlite3_backup *bk = sqlite3_backup_init(backup_db, "main", app->db, "main");
    if (!bk) {
        fprintf(stderr, "Backup: Init failed: %s\n", sqlite3_errmsg(backup_db));
        sqlite3_close(backup_db);
        remove(tmp_path);
        return NULL;
    }

    rc = sqlite3_backup_step(bk, -1);
    int pages_total = sqlite3_backup_pagecount(bk);
    sqlite3_backup_finish(bk);
    sqlite3_close(backup_db);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Backup: Step failed with code %d\n", rc);
        remove(tmp_path);
        return NULL;
    }
    printf("[BACKUP] %d pages written to temp file\n", pages_total);

    // 3. Encrypt if passphrase provided, otherwise just rename
    if (passphrase && strlen(passphrase) > 0) {
        FILE *f = fopen(tmp_path, "rb");
        if (!f) { remove(tmp_path); return NULL; }

        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET);

        unsigned char *buffer = malloc(fsize);
        if (!buffer) { fclose(f); remove(tmp_path); return NULL; }
        fread(buffer, 1, fsize, f);
        fclose(f);
        remove(tmp_path);

        bool ok = write_encrypted_backup(filepath, buffer, fsize, passphrase);
        free(buffer);

        if (!ok) return NULL;
    } else {
        // No passphrase — just save as plain SQLite file
        rename(tmp_path, filepath);
        printf("[BACKUP] Saved unencrypted backup: %s\n", filepath);
    }

    // 4. Log the backup
    FILE *check = fopen(filepath, "rb");
    long final_size = 0;
    if (check) {
        fseek(check, 0, SEEK_END);
        final_size = ftell(check);
        fclose(check);
    }

    cJSON *gen_q = db_query(app, "SELECT COUNT(*) as count FROM backup_log;");
    int generation = 1;
    if (gen_q && cJSON_GetArraySize(gen_q) > 0) {
        cJSON *row = cJSON_GetArrayItem(gen_q, 0);
        cJSON *cnt = cJSON_GetObjectItem(row, "count");
        if (cnt) generation = (int)cnt->valuedouble + 1;
    }
    cJSON_Delete(gen_q);

    sqlite3_stmt *log_stmt;
    const char *log_sql = "INSERT INTO backup_log (filename, size_bytes, encrypted, generation, created_at) "
                          "VALUES (?, ?, ?, ?, strftime('%s','now'));";
    if (sqlite3_prepare_v2(app->db, log_sql, -1, &log_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(log_stmt, 1, filepath, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(log_stmt, 2, final_size);
        sqlite3_bind_int(log_stmt, 3, (passphrase && strlen(passphrase) > 0) ? 1 : 0);
        sqlite3_bind_int(log_stmt, 4, generation);
        sqlite3_step(log_stmt);
        sqlite3_finalize(log_stmt);
    }

    // Auto-prune: keep max 15 generations
    db_execute(app, "DELETE FROM backup_log WHERE id NOT IN "
                    "(SELECT id FROM backup_log ORDER BY created_at DESC LIMIT 15);");

    return filepath;
}

// ============================================================
// Restore from a backup file
// ============================================================
bool backup_restore(AppState *app, const char *filepath, const char *passphrase) {
    if (!filepath) return false;

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        fprintf(stderr, "Restore: Cannot open %s\n", filepath);
        return false;
    }

    // Detect format by magic header
    char magic[BACKUP_MAGIC_LEN];
    bool is_encrypted = false;
    if (fread(magic, 1, BACKUP_MAGIC_LEN, f) == BACKUP_MAGIC_LEN) {
        is_encrypted = (memcmp(magic, BACKUP_MAGIC, BACKUP_MAGIC_LEN) == 0);
    }
    fclose(f);

    char temp_path[560];
    snprintf(temp_path, sizeof(temp_path), "%s.restore.tmp", filepath);

    if (is_encrypted) {
        if (!passphrase || strlen(passphrase) == 0) {
            fprintf(stderr, "Restore: File is AES-256-GCM encrypted but no passphrase provided\n");
            return false;
        }

        size_t plain_len = 0;
        unsigned char *plain = read_encrypted_backup(filepath, passphrase, &plain_len);
        if (!plain) return false;

        // Write decrypted SQLite database to temp file
        f = fopen(temp_path, "wb");
        if (!f) { free(plain); return false; }
        fwrite(plain, 1, plain_len, f);
        fclose(f);
        free(plain);
    } else {
        // Unencrypted plain SQLite backup
        snprintf(temp_path, sizeof(temp_path), "%s", filepath);
    }

    // Restore into running database using SQLite Backup API
    sqlite3 *src_db;
    int rc = sqlite3_open(temp_path, &src_db);
    if (rc != SQLITE_OK) {
        if (is_encrypted) remove(temp_path);
        return false;
    }

    sqlite3_backup *bk = sqlite3_backup_init(app->db, "main", src_db, "main");
    bool ok = false;
    if (bk) {
        ok = (sqlite3_backup_step(bk, -1) == SQLITE_DONE);
        sqlite3_backup_finish(bk);
    }
    sqlite3_close(src_db);

    if (is_encrypted) remove(temp_path);

    if (ok) printf("[BACKUP] Successfully restored from %s\n", filepath);
    else    fprintf(stderr, "Restore: SQLite restore step failed\n");

    return ok;
}

// ============================================================
// List recent backups
// ============================================================
cJSON* backup_list(AppState *app) {
    return db_query(app,
        "SELECT * FROM backup_log ORDER BY created_at DESC LIMIT 20;");
}

// ============================================================
// API Routes
// ============================================================

void route_backup_create(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    cJSON *req = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
    const char *pass = "";
    if (req) {
        cJSON *p = cJSON_GetObjectItem(req, "passphrase");
        if (p && cJSON_IsString(p)) pass = p->valuestring;
    }

    mkdir(BACKUP_DIR, 0755);

    char *path = backup_create(app, BACKUP_DIR, pass);
    if (req) cJSON_Delete(req);

    if (path) {
        cJSON *res = cJSON_CreateObject();
        cJSON_AddStringToObject(res, "status", "success");
        cJSON_AddStringToObject(res, "path", path);
        cJSON_AddStringToObject(res, "encryption", "AES-256-GCM");
        send_json(c, 200, res);
    } else {
        send_error(c, 500, "Backup creation failed");
    }
}

void route_backup_list(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    cJSON *list = backup_list(app);
    send_json(c, 200, list);
}
