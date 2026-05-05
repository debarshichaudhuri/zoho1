#include "../include/qmanage.h"

// ============================================================
// Record a balanced Double-Entry Journal Entry
// ALL amounts in PAISA (int64_t) — zero floating-point error
// ============================================================
FinResult ledger_record_entry(AppState *app, const char *memo, int64_t timestamp, cJSON *lines) {
    if (!cJSON_IsArray(lines)) return FIN_ERR_DB;

    // 1. Validate Balance — exact integer comparison (no fabs tolerance!)
    int64_t total_debit = 0, total_credit = 0;
    cJSON *line;
    cJSON_ArrayForEach(line, lines) {
        cJSON *d = cJSON_GetObjectItem(line, "debit");
        cJSON *c = cJSON_GetObjectItem(line, "credit");
        if (d) total_debit += (int64_t)d->valuedouble;
        if (c) total_credit += (int64_t)c->valuedouble;
    }

    // Exact match required — no rounding tolerance
    if (total_debit != total_credit) {
        printf("[LEDGER] REJECTED: Debit=%ld != Credit=%ld (diff=%ld paisa)\n",
               (long long)total_debit, (long long)total_credit,
               (long long)(total_debit - total_credit));
        return FIN_ERR_BALANCE;
    }

    // 2. Start Atomic Transaction
    sqlite3_exec(app->db, "BEGIN TRANSACTION;", 0, 0, 0);

    // 3. Insert Journal Entry Header
    sqlite3_stmt *stmt;
    const char *h_sql = "INSERT INTO journal_entries (entry_date, memo, source_module, created_at) "
                        "VALUES (?, ?, 'LEDGER', strftime('%s','now'));";
    if (sqlite3_prepare_v2(app->db, h_sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_exec(app->db, "ROLLBACK;", 0, 0, 0);
        return FIN_ERR_DB;
    }

    sqlite3_bind_int64(stmt, 1, timestamp);
    sqlite3_bind_text(stmt, 2, memo, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        sqlite3_exec(app->db, "ROLLBACK;", 0, 0, 0);
        return FIN_ERR_DB;
    }

    int64_t journal_id = sqlite3_last_insert_rowid(app->db);
    sqlite3_finalize(stmt);

    // 4. Insert Journal Lines — int64 bind for debit/credit
    const char *l_sql = "INSERT INTO journal_lines (journal_id, account_id, debit, credit, memo) "
                        "VALUES (?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(app->db, l_sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_exec(app->db, "ROLLBACK;", 0, 0, 0);
        return FIN_ERR_DB;
    }

    cJSON_ArrayForEach(line, lines) {
        sqlite3_reset(stmt);
        sqlite3_bind_int64(stmt, 1, journal_id);
        sqlite3_bind_int64(stmt, 2, (int64_t)cJSON_GetObjectItem(line, "account_id")->valuedouble);
        sqlite3_bind_int64(stmt, 3, (int64_t)cJSON_GetObjectItem(line, "debit")->valuedouble);
        sqlite3_bind_int64(stmt, 4, (int64_t)cJSON_GetObjectItem(line, "credit")->valuedouble);

        cJSON *line_memo = cJSON_GetObjectItem(line, "memo");
        sqlite3_bind_text(stmt, 5, (line_memo && cJSON_IsString(line_memo)) ? line_memo->valuestring : "", -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            sqlite3_exec(app->db, "ROLLBACK;", 0, 0, 0);
            return FIN_ERR_DB;
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(app->db, "COMMIT;", 0, 0, 0);

    printf("[LEDGER] Entry #%ld: %s | Debit=%ld Credit=%ld (BALANCED)\n",
           (long long)journal_id, memo, (long long)total_debit, (long long)total_credit);

    return FIN_OK;
}

// ============================================================
// Calculate Balance for an Account (in paisa)
// ============================================================
int64_t ledger_get_account_balance_paisa(AppState *app, int64_t account_id, int64_t start_date, int64_t end_date) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT COALESCE(SUM(debit), 0) - COALESCE(SUM(credit), 0) "
                      "FROM journal_lines l "
                      "JOIN journal_entries e ON l.journal_id = e.id "
                      "WHERE l.account_id = ? AND e.entry_date BETWEEN ? AND ?;";

    if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;

    sqlite3_bind_int64(stmt, 1, account_id);
    sqlite3_bind_int64(stmt, 2, start_date);
    sqlite3_bind_int64(stmt, 3, end_date);

    int64_t balance = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        balance = sqlite3_column_int64(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return balance;
}

// Legacy wrapper for backward compatibility
double ledger_get_account_balance(AppState *app, int64_t account_id, int64_t start_date, int64_t end_date) {
    return (double)ledger_get_account_balance_paisa(app, account_id, start_date, end_date);
}
