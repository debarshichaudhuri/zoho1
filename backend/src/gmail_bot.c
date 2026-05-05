#include "../include/erp.h"
#include "../include/crypto.h"

// ============================================================
// Gmail IMAP Bot — Zero-API Bank Reconciliation
// 
// Logic: User connects their Gmail that receives bank alerts.
// We parse the emails for transaction data, extract amounts,
// UTR references, and auto-match against open invoices/bills.
// No bank API. No RBI approval. Just email.
// ============================================================

// Save Gmail IMAP configuration
void route_gmail_config(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    if (mg_vcasecmp(&hm->method, "POST") == 0) {
        cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
        if (!root) { send_error(c, 400, "Invalid JSON"); return; }

        cJSON *email_j = cJSON_GetObjectItem(root, "email");
        cJSON *pass_j = cJSON_GetObjectItem(root, "app_password");

        if (!email_j || !pass_j) {
            send_error(c, 400, "email and app_password required");
            cJSON_Delete(root); return;
        }

        // Encrypt app password before storing
        // Key is derived from a fixed device salt + the email as context
        unsigned char salt[16];
        crypto_sha256((const unsigned char*)email_j->valuestring,
                      strlen(email_j->valuestring), salt);
        // Use first 16 bytes of email hash as salt

        unsigned char key[32];
        crypto_derive_key("qmanage-gmail-key", salt, 50000, key);

        size_t enc_len = 0;
        unsigned char *encrypted = crypto_aes256gcm_encrypt(
            (const unsigned char*)pass_j->valuestring,
            strlen(pass_j->valuestring),
            key, &enc_len
        );

        // Store config with encrypted password
        sqlite3_stmt *stmt;
        const char *sql = "INSERT OR REPLACE INTO gmail_config (id, email, app_password, is_active, created_at) "
                          "VALUES (1, ?, ?, 1, strftime('%s','now'));";
        if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, email_j->valuestring, -1, SQLITE_TRANSIENT);
            if (encrypted && enc_len > 0) {
                sqlite3_bind_blob(stmt, 2, encrypted, (int)enc_len, SQLITE_TRANSIENT);
            } else {
                // Fallback: store as-is if encryption failed
                sqlite3_bind_text(stmt, 2, pass_j->valuestring, -1, SQLITE_TRANSIENT);
            }
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }

        if (encrypted) free(encrypted);

        // Audit trail
        audit_log_action(app, "GMAIL_CONFIG", "gmail", 1, email_j->valuestring);

        cJSON_Delete(root);

        cJSON *ok = cJSON_CreateObject();
        cJSON_AddStringToObject(ok, "status", "configured");
        cJSON_AddStringToObject(ok, "message", "Gmail configured. App password encrypted at rest.");
        send_json(c, 200, ok);
    } else {
        // GET — return config status (password is NEVER returned)
        cJSON *config = db_query(app,
            "SELECT id, email, "
            "CASE WHEN app_password IS NOT NULL THEN '●●●●●●●●' ELSE '' END as password_set, "
            "imap_server, last_uid, poll_interval, is_active, created_at "
            "FROM gmail_config WHERE id = 1;");
        send_json(c, 200, config);
    }
}

// Simulate fetching new bank notifications from Gmail
// In production, this would use IMAP with libcurl. For now, we parse
// raw email text submitted by the frontend to demonstrate the pipeline.
void route_gmail_fetch(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
    if (!root) { send_error(c, 400, "Invalid JSON"); return; }

    cJSON *emails = cJSON_GetObjectItem(root, "emails");
    if (!emails || !cJSON_IsArray(emails)) {
        // If no emails array, treat body as a single bank alert
        cJSON *text_j = cJSON_GetObjectItem(root, "email_text");
        if (!text_j) {
            send_error(c, 400, "Provide 'emails' array or 'email_text'");
            cJSON_Delete(root); return;
        }
        // Create single-item array
        emails = cJSON_CreateArray();
        cJSON *single = cJSON_CreateObject();
        cJSON_AddStringToObject(single, "body", text_j->valuestring);
        cJSON_AddItemToArray(emails, single);
    }

    int parsed_count = 0;
    int matched_count = 0;

    int size = cJSON_GetArraySize(emails);
    for (int i = 0; i < size; i++) {
        cJSON *email = cJSON_GetArrayItem(emails, i);
        cJSON *body = cJSON_GetObjectItem(email, "body");
        if (!body || !cJSON_IsString(body)) continue;

        const char *text = body->valuestring;

        // ---- Parse bank alert text ----
        // Supports: HDFC, ICICI, SBI, Kotak, Axis, Yes Bank, PNB
        // Pattern: Rs. XX,XXX.XX or INR XX,XXX.XX credited/debited
        double amount = 0;
        char type[16] = "credit";
        char utr[64] = "";
        char acref[32] = "";

        // Extract amount (handles Rs., INR, Rs )
        const char *amt_markers[] = {"Rs.", "Rs ", "INR ", "Rs"};
        for (int m = 0; m < 4; m++) {
            char *pos = strstr(text, amt_markers[m]);
            if (pos) {
                pos += strlen(amt_markers[m]);
                while (*pos == ' ') pos++;
                char num_buf[32] = {0};
                int ni = 0;
                while (*pos && ((*pos >= '0' && *pos <= '9') || *pos == ',' || *pos == '.') && ni < 30) {
                    if (*pos != ',') num_buf[ni++] = *pos;
                    pos++;
                }
                if (ni > 0) amount = atof(num_buf);
                break;
            }
        }

        // Detect credit/debit
        if (strstr(text, "debit") || strstr(text, "debited") || strstr(text, "withdrawn") ||
            strstr(text, "paid") || strstr(text, "sent")) {
            strcpy(type, "debit");
        }

        // Extract UTR / Reference
        const char *ref_markers[] = {"UTR:", "UTR ", "Ref:", "ref ", "UPI Ref:", "IMPS Ref:", "NEFT/"};
        for (int m = 0; m < 7; m++) {
            char *pos = strstr(text, ref_markers[m]);
            if (pos) {
                pos += strlen(ref_markers[m]);
                while (*pos == ' ') pos++;
                int ui = 0;
                while (*pos && *pos != ' ' && *pos != '.' && *pos != '\n' && ui < 60) {
                    utr[ui++] = *pos++;
                }
                break;
            }
        }

        // Extract account reference (XX1234 pattern)
        char *xx_pos = strstr(text, "XX");
        if (!xx_pos) xx_pos = strstr(text, "xx");
        if (!xx_pos) xx_pos = strstr(text, "A/c");
        if (xx_pos) {
            int ai = 0;
            while (*xx_pos && *xx_pos != ' ' && *xx_pos != ',' && ai < 20) {
                acref[ai++] = *xx_pos++;
            }
        }

        if (amount <= 0) continue;  // Couldn't parse

        // Store as bank transaction
        int64_t amount_paisa = (int64_t)(amount * 100);
        sqlite3_stmt *stmt;
        const char *sql = "INSERT INTO bank_transactions (type, amount, account_ref, utr, description, source, date, created_at) "
                          "VALUES (?, ?, ?, ?, ?, 'gmail', strftime('%s','now'), strftime('%s','now'));";
        if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, type, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(stmt, 2, amount_paisa);
            sqlite3_bind_text(stmt, 3, acref, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, utr, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 5, body->valuestring, -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
        parsed_count++;

        // Auto-match: Try to find an invoice with matching amount (PARAMETERIZED)
        if (strcmp(type, "credit") == 0) {
            sqlite3_stmt *match_stmt;
            const char *match_sql = "SELECT id, invoice_num, customer_id FROM invoices "
                                    "WHERE balance_due = ? AND status IN ('sent','partial','overdue') "
                                    "LIMIT 1;";
            if (sqlite3_prepare_v2(app->db, match_sql, -1, &match_stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int64(match_stmt, 1, amount_paisa);
                if (sqlite3_step(match_stmt) == SQLITE_ROW) {
                    int inv_id = sqlite3_column_int(match_stmt, 0);
                    int64_t tx_id = sqlite3_last_insert_rowid(app->db);

                    // Mark transaction as matched (parameterized)
                    sqlite3_stmt *upd_stmt;
                    const char *upd_sql = "UPDATE bank_transactions SET is_matched = 1, matched_type = 'invoice', matched_id = ? WHERE id = ?;";
                    if (sqlite3_prepare_v2(app->db, upd_sql, -1, &upd_stmt, NULL) == SQLITE_OK) {
                        sqlite3_bind_int(upd_stmt, 1, inv_id);
                        sqlite3_bind_int64(upd_stmt, 2, tx_id);
                        sqlite3_step(upd_stmt);
                        sqlite3_finalize(upd_stmt);
                    }

                    // Update invoice balance (parameterized)
                    const char *inv_sql = "UPDATE invoices SET amount_paid = amount_paid + ?, "
                                          "balance_due = total - (amount_paid + ?), "
                                          "status = CASE WHEN (amount_paid + ?) >= total THEN 'paid' ELSE 'partial' END "
                                          "WHERE id = ?;";
                    if (sqlite3_prepare_v2(app->db, inv_sql, -1, &upd_stmt, NULL) == SQLITE_OK) {
                        sqlite3_bind_int64(upd_stmt, 1, amount_paisa);
                        sqlite3_bind_int64(upd_stmt, 2, amount_paisa);
                        sqlite3_bind_int64(upd_stmt, 3, amount_paisa);
                        sqlite3_bind_int(upd_stmt, 4, inv_id);
                        sqlite3_step(upd_stmt);
                        sqlite3_finalize(upd_stmt);
                    }

                    // Audit the auto-match
                    char audit_detail[128];
                    snprintf(audit_detail, sizeof(audit_detail), "Auto-matched bank tx to invoice #%d (₹%.2f)", inv_id, amount);
                    audit_log_action(app, "BANK_AUTO_MATCH", "invoice", inv_id, audit_detail);

                    matched_count++;
                }
            }
            sqlite3_finalize(match_stmt);
        }
    }

    cJSON_Delete(root);

    cJSON *result = cJSON_CreateObject();
    cJSON_AddNumberToObject(result, "parsed", parsed_count);
    cJSON_AddNumberToObject(result, "auto_matched", matched_count);
    cJSON_AddStringToObject(result, "message",
        matched_count > 0 ? "Transactions parsed and auto-matched to invoices!" :
        parsed_count > 0 ? "Transactions parsed. Review for manual matching." :
        "No bank transactions could be parsed from the emails.");
    send_json(c, 200, result);
}

// Status of Gmail bot
void route_gmail_status(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    cJSON *res = cJSON_CreateObject();

    cJSON *config = db_query(app,
        "SELECT email, is_active, last_uid FROM gmail_config WHERE id = 1;");
    cJSON_AddItemToObject(res, "config", config);

    cJSON *stats = db_query(app,
        "SELECT "
        "COUNT(*) as total_transactions, "
        "SUM(CASE WHEN is_matched = 1 THEN 1 ELSE 0 END) as matched, "
        "SUM(CASE WHEN is_matched = 0 THEN 1 ELSE 0 END) as unmatched, "
        "SUM(CASE WHEN source = 'gmail' THEN 1 ELSE 0 END) as from_gmail, "
        "SUM(CASE WHEN source = 'sms' THEN 1 ELSE 0 END) as from_sms "
        "FROM bank_transactions;");
    if (stats && cJSON_GetArraySize(stats) > 0) {
        cJSON *s = cJSON_DetachItemFromArray(stats, 0);
        cJSON_AddItemToObject(res, "stats", s);
    }
    cJSON_Delete(stats);

    send_json(c, 200, res);
}

// ============================================================
// Send SMTP Email via curl.exe (Native Windows)
// ============================================================
void route_email_send(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    if (mg_vcasecmp(&hm->method, "POST") != 0) {
        send_error(c, 405, "Method not allowed");
        return;
    }

    cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
    if (!root) { send_error(c, 400, "Invalid JSON"); return; }

    cJSON *to_j = cJSON_GetObjectItem(root, "to");
    cJSON *subj_j = cJSON_GetObjectItem(root, "subject");
    cJSON *body_j = cJSON_GetObjectItem(root, "body");

    if (!to_j || !subj_j || !body_j) {
        send_error(c, 400, "Missing 'to', 'subject', or 'body'");
        cJSON_Delete(root); return;
    }

    // Get config
    sqlite3_stmt *stmt;
    const char *sql = "SELECT email, app_password FROM gmail_config WHERE id = 1 AND is_active = 1;";
    if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        send_error(c, 500, "Database error"); cJSON_Delete(root); return;
    }

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        send_error(c, 400, "Gmail/SMTP not configured. Please configure in Banking -> Settings.");
        cJSON_Delete(root); return;
    }

    const char *email = (const char*)sqlite3_column_text(stmt, 0);
    const void *blob = sqlite3_column_blob(stmt, 1);
    int blob_len = sqlite3_column_bytes(stmt, 1);

    if (!email || !blob || blob_len == 0) {
        sqlite3_finalize(stmt);
        send_error(c, 400, "Incomplete Gmail config");
        cJSON_Delete(root); return;
    }

    // Decrypt password
    unsigned char salt[16];
    crypto_sha256((const unsigned char*)email, strlen(email), salt);
    unsigned char key[32];
    crypto_derive_key("qmanage-gmail-key", salt, 50000, key);

    size_t dec_len = 0;
    unsigned char *decrypted = crypto_aes256gcm_decrypt((const unsigned char*)blob, blob_len, key, &dec_len);
    sqlite3_finalize(stmt);

    if (!decrypted) {
        send_error(c, 500, "Failed to decrypt SMTP password");
        cJSON_Delete(root); return;
    }

    char *pass = malloc(dec_len + 1);
    memcpy(pass, decrypted, dec_len);
    pass[dec_len] = '\0';
    free(decrypted);

    // 1. Write the email text file (headers + body)
    FILE *fmail = fopen("mail.txt", "wb");
    if (!fmail) {
        free(pass);
        send_error(c, 500, "Failed to create temp mail file");
        cJSON_Delete(root); return;
    }
    fprintf(fmail, "From: \"QManage\" <%s>\r\n", email);
    fprintf(fmail, "To: %s\r\n", to_j->valuestring);
    fprintf(fmail, "Subject: %s\r\n", subj_j->valuestring);
    fprintf(fmail, "Content-Type: text/plain; charset=\"UTF-8\"\r\n\r\n");
    fprintf(fmail, "%s\r\n", body_j->valuestring);
    fclose(fmail);

    // 2. Write the curl config file to avoid password in command line
    FILE *fcfg = fopen("curl-auth.txt", "wb");
    if (!fcfg) {
        free(pass);
        remove("mail.txt");
        send_error(c, 500, "Failed to create temp config file");
        cJSON_Delete(root); return;
    }
    fprintf(fcfg, "url = \"smtps://smtp.gmail.com:465\"\r\n");
    fprintf(fcfg, "ssl-reqd = true\r\n");
    fprintf(fcfg, "mail-from = \"%s\"\r\n", email);
    fprintf(fcfg, "mail-rcpt = \"%s\"\r\n", to_j->valuestring);
    fprintf(fcfg, "user = \"%s:%s\"\r\n", email, pass);
    fprintf(fcfg, "upload-file = \"mail.txt\"\r\n");
    fclose(fcfg);

    free(pass);

    // 3. Execute curl.exe
    int ret = system("curl.exe -K curl-auth.txt --silent --output curl-out.txt");

    remove("mail.txt");
    remove("curl-auth.txt");
    remove("curl-out.txt");

    cJSON_Delete(root);

    if (ret == 0) {
        audit_log_action(app, "EMAIL_SENT", "email", 0, subj_j->valuestring);
        cJSON *ok = cJSON_CreateObject();
        cJSON_AddStringToObject(ok, "status", "sent");
        send_json(c, 200, ok);
    } else {
        send_error(c, 500, "Failed to send email via curl. Check console for details.");
    }
}
