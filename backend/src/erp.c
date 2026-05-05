#include "../include/erp.h"
#include "../include/crypto.h"
#include <time.h>

// ============================================================
// BILLS — Vendor Invoices (Accounts Payable)
// ============================================================
void route_bills(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    if (mg_vcasecmp(&hm->method, "POST") == 0) {
        cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
        if (!root) { send_error(c, 400, "Invalid JSON"); return; }

        cJSON *num = cJSON_GetObjectItem(root, "bill_num");
        cJSON *vendor = cJSON_GetObjectItem(root, "vendor_id");
        cJSON *total_j = cJSON_GetObjectItem(root, "total");
        cJSON *date_j = cJSON_GetObjectItem(root, "date");
        cJSON *notes_j = cJSON_GetObjectItem(root, "notes");

        if (!num || !vendor || !total_j) {
            send_error(c, 400, "Missing bill_num, vendor_id, or total");
            cJSON_Delete(root); return;
        }

        int64_t total = (int64_t)(total_j->valuedouble);
        int64_t date = date_j ? (int64_t)date_j->valuedouble : (int64_t)time(NULL);

        // 1. Insert bill
        sqlite3_stmt *stmt;
        const char *sql = "INSERT INTO bills (bill_num, vendor_id, total, balance_due, date, status, notes, created_at) VALUES (?, ?, ?, ?, ?, 'received', ?, strftime('%s','now'));";
        if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, num->valuestring, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 2, vendor->valueint);
            sqlite3_bind_int64(stmt, 3, total);
            sqlite3_bind_int64(stmt, 4, total);
            sqlite3_bind_int64(stmt, 5, date);
            sqlite3_bind_text(stmt, 6, (notes_j && cJSON_IsString(notes_j)) ? notes_j->valuestring : "", -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }

        // 2. Auto-create ledger entry: Debit Expense(8), Credit AP(5)
        cJSON *lines = cJSON_CreateArray();
        cJSON *dr = cJSON_CreateObject();
        cJSON_AddNumberToObject(dr, "account_id", 8);
        cJSON_AddNumberToObject(dr, "debit", total);
        cJSON_AddNumberToObject(dr, "credit", 0);
        cJSON_AddItemToArray(lines, dr);

        cJSON *cr = cJSON_CreateObject();
        cJSON_AddNumberToObject(cr, "account_id", 5);
        cJSON_AddNumberToObject(cr, "debit", 0);
        cJSON_AddNumberToObject(cr, "credit", total);
        cJSON_AddItemToArray(lines, cr);

        char memo[128];
        snprintf(memo, sizeof(memo), "Bill %s", num->valuestring);
        ledger_record_entry(app, memo, date, lines);
        cJSON_Delete(lines);
        cJSON_Delete(root);

        cJSON *ok = cJSON_CreateObject();
        cJSON_AddStringToObject(ok, "status", "created");
        send_json(c, 201, ok);
    } else {
        int limit, offset;
        get_pagination(hm, &limit, &offset);
        char sql[512];
        snprintf(sql, sizeof(sql),
            "SELECT b.*, c.display_name as vendor_name "
            "FROM bills b LEFT JOIN contacts c ON b.vendor_id = c.id "
            "ORDER BY b.date DESC LIMIT %d OFFSET %d;", limit, offset);
        send_json(c, 200, db_query(app, sql));
    }
}

// ============================================================
// PAYMENTS RECEIVED — Against Invoices
// ============================================================
void route_payments_received(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    if (mg_vcasecmp(&hm->method, "POST") == 0) {
        cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
        if (!root) { send_error(c, 400, "Invalid JSON"); return; }

        cJSON *cust_j = cJSON_GetObjectItem(root, "customer_id");
        cJSON *inv_j = cJSON_GetObjectItem(root, "invoice_id");
        cJSON *amt_j = cJSON_GetObjectItem(root, "amount");
        cJSON *date_j = cJSON_GetObjectItem(root, "date");
        cJSON *mode_j = cJSON_GetObjectItem(root, "mode");
        cJSON *ref_j = cJSON_GetObjectItem(root, "reference");

        if (!cust_j || !amt_j) {
            send_error(c, 400, "customer_id and amount required");
            cJSON_Delete(root); return;
        }

        int64_t amount = (int64_t)(amt_j->valuedouble);
        int64_t date = date_j ? (int64_t)date_j->valuedouble : (int64_t)time(NULL);

        // 1. Insert payment record
        sqlite3_stmt *stmt;
        const char *sql = "INSERT INTO payments_received (payment_num, customer_id, invoice_id, amount, date, mode, reference, created_at) "
                          "VALUES (?, ?, ?, ?, ?, ?, ?, strftime('%s','now'));";
        if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            char pnum[32];
            snprintf(pnum, sizeof(pnum), "PAY-%ld", (long long)time(NULL) % 100000);
            sqlite3_bind_text(stmt, 1, pnum, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 2, cust_j->valueint);
            sqlite3_bind_int(stmt, 3, (inv_j && cJSON_IsNumber(inv_j)) ? inv_j->valueint : 0);
            sqlite3_bind_int64(stmt, 4, amount);
            sqlite3_bind_int64(stmt, 5, date);
            sqlite3_bind_text(stmt, 6, (mode_j && cJSON_IsString(mode_j)) ? mode_j->valuestring : "bank_transfer", -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 7, (ref_j && cJSON_IsString(ref_j)) ? ref_j->valuestring : "", -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }

        // 2. Update invoice balance (PARAMETERIZED)
        if (inv_j && cJSON_IsNumber(inv_j) && inv_j->valueint > 0) {
            sqlite3_stmt *upd;
            const char *upd_sql = "UPDATE invoices SET amount_paid = amount_paid + ?, "
                                  "balance_due = total - (amount_paid + ?), "
                                  "status = CASE WHEN (amount_paid + ?) >= total THEN 'paid' "
                                  "WHEN (amount_paid + ?) > 0 THEN 'partial' ELSE status END "
                                  "WHERE id = ?;";
            if (sqlite3_prepare_v2(app->db, upd_sql, -1, &upd, NULL) == SQLITE_OK) {
                sqlite3_bind_int64(upd, 1, amount);
                sqlite3_bind_int64(upd, 2, amount);
                sqlite3_bind_int64(upd, 3, amount);
                sqlite3_bind_int64(upd, 4, amount);
                sqlite3_bind_int(upd, 5, inv_j->valueint);
                sqlite3_step(upd);
                sqlite3_finalize(upd);
            }
            audit_log_action(app, "PAYMENT_RECEIVED", "invoice", inv_j->valueint, "Payment applied");
        }

        // 3. Ledger entry: Debit Bank(2), Credit AR(4)
        cJSON *lines = cJSON_CreateArray();
        cJSON *dr = cJSON_CreateObject();
        cJSON_AddNumberToObject(dr, "account_id", 2);
        cJSON_AddNumberToObject(dr, "debit", amount);
        cJSON_AddNumberToObject(dr, "credit", 0);
        cJSON_AddItemToArray(lines, dr);

        cJSON *cr = cJSON_CreateObject();
        cJSON_AddNumberToObject(cr, "account_id", 4);
        cJSON_AddNumberToObject(cr, "debit", 0);
        cJSON_AddNumberToObject(cr, "credit", amount);
        cJSON_AddItemToArray(lines, cr);

        ledger_record_entry(app, "Payment received", date, lines);
        cJSON_Delete(lines);
        cJSON_Delete(root);

        cJSON *ok = cJSON_CreateObject();
        cJSON_AddStringToObject(ok, "status", "recorded");
        send_json(c, 201, ok);
    } else {
        int limit, offset;
        get_pagination(hm, &limit, &offset);
        char sql[512];
        snprintf(sql, sizeof(sql),
            "SELECT p.*, c.display_name as customer_name "
            "FROM payments_received p LEFT JOIN contacts c ON p.customer_id = c.id "
            "ORDER BY p.date DESC LIMIT %d OFFSET %d;", limit, offset);
        send_json(c, 200, db_query(app, sql));
    }
}

// ============================================================
// PAYMENTS MADE — Against Bills  
// ============================================================
void route_payments_made(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    if (mg_vcasecmp(&hm->method, "POST") == 0) {
        cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
        if (!root) { send_error(c, 400, "Invalid JSON"); return; }

        cJSON *vendor_j = cJSON_GetObjectItem(root, "vendor_id");
        cJSON *bill_j = cJSON_GetObjectItem(root, "bill_id");
        cJSON *amt_j = cJSON_GetObjectItem(root, "amount");
        cJSON *date_j = cJSON_GetObjectItem(root, "date");
        cJSON *mode_j = cJSON_GetObjectItem(root, "mode");

        if (!vendor_j || !amt_j) {
            send_error(c, 400, "vendor_id and amount required");
            cJSON_Delete(root); return;
        }

        int64_t amount = (int64_t)(amt_j->valuedouble);
        int64_t date = date_j ? (int64_t)date_j->valuedouble : (int64_t)time(NULL);

        // Insert payment
        sqlite3_stmt *stmt;
        const char *sql = "INSERT INTO payments_made (payment_num, vendor_id, bill_id, amount, date, mode, created_at) "
                          "VALUES (?, ?, ?, ?, ?, ?, strftime('%s','now'));";
        if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            char pnum[32];
            snprintf(pnum, sizeof(pnum), "PMADE-%ld", (long long)time(NULL) % 100000);
            sqlite3_bind_text(stmt, 1, pnum, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 2, vendor_j->valueint);
            sqlite3_bind_int(stmt, 3, (bill_j && cJSON_IsNumber(bill_j)) ? bill_j->valueint : 0);
            sqlite3_bind_int64(stmt, 4, amount);
            sqlite3_bind_int64(stmt, 5, date);
            sqlite3_bind_text(stmt, 6, (mode_j && cJSON_IsString(mode_j)) ? mode_j->valuestring : "bank_transfer", -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }

        // Update bill status (PARAMETERIZED)
        if (bill_j && cJSON_IsNumber(bill_j) && bill_j->valueint > 0) {
            sqlite3_stmt *upd;
            const char *upd_sql = "UPDATE bills SET amount_paid = amount_paid + ?, "
                                  "balance_due = total - (amount_paid + ?), "
                                  "status = CASE WHEN (amount_paid + ?) >= total THEN 'paid' "
                                  "WHEN (amount_paid + ?) > 0 THEN 'partial' ELSE status END "
                                  "WHERE id = ?;";
            if (sqlite3_prepare_v2(app->db, upd_sql, -1, &upd, NULL) == SQLITE_OK) {
                sqlite3_bind_int64(upd, 1, amount);
                sqlite3_bind_int64(upd, 2, amount);
                sqlite3_bind_int64(upd, 3, amount);
                sqlite3_bind_int64(upd, 4, amount);
                sqlite3_bind_int(upd, 5, bill_j->valueint);
                sqlite3_step(upd);
                sqlite3_finalize(upd);
            }
            audit_log_action(app, "PAYMENT_MADE", "bill", bill_j->valueint, "Payment applied");
        }

        // Ledger: Debit AP(5), Credit Bank(2)
        cJSON *lines = cJSON_CreateArray();
        cJSON *dr = cJSON_CreateObject();
        cJSON_AddNumberToObject(dr, "account_id", 5);
        cJSON_AddNumberToObject(dr, "debit", amount);
        cJSON_AddNumberToObject(dr, "credit", 0);
        cJSON_AddItemToArray(lines, dr);

        cJSON *cr = cJSON_CreateObject();
        cJSON_AddNumberToObject(cr, "account_id", 2);
        cJSON_AddNumberToObject(cr, "debit", 0);
        cJSON_AddNumberToObject(cr, "credit", amount);
        cJSON_AddItemToArray(lines, cr);

        ledger_record_entry(app, "Payment made", date, lines);
        cJSON_Delete(lines);
        cJSON_Delete(root);

        cJSON *ok = cJSON_CreateObject();
        cJSON_AddStringToObject(ok, "status", "recorded");
        send_json(c, 201, ok);
    } else {
        int limit, offset;
        get_pagination(hm, &limit, &offset);
        char sql[512];
        snprintf(sql, sizeof(sql),
            "SELECT p.*, c.display_name as vendor_name "
            "FROM payments_made p LEFT JOIN contacts c ON p.vendor_id = c.id "
            "ORDER BY p.date DESC LIMIT %d OFFSET %d;", limit, offset);
        send_json(c, 200, db_query(app, sql));
    }
}

// ============================================================
// QUOTES / ESTIMATES
// ============================================================
void route_quotes(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    if (mg_vcasecmp(&hm->method, "POST") == 0) {
        cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
        if (!root) { send_error(c, 400, "Invalid JSON"); return; }

        cJSON *num = cJSON_GetObjectItem(root, "quote_num");
        cJSON *cust = cJSON_GetObjectItem(root, "customer_id");
        cJSON *total_j = cJSON_GetObjectItem(root, "total");
        cJSON *date_j = cJSON_GetObjectItem(root, "date");

        if (!num || !cust || !total_j) {
            send_error(c, 400, "Missing quote_num, customer_id, or total");
            cJSON_Delete(root); return;
        }

        sqlite3_stmt *stmt;
        const char *sql = "INSERT INTO quotes (quote_num, customer_id, total, date, status, created_at) "
                          "VALUES (?, ?, ?, ?, 'draft', strftime('%s','now'));";
        if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, num->valuestring, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 2, cust->valueint);
            sqlite3_bind_int64(stmt, 3, (int64_t)total_j->valuedouble);
            sqlite3_bind_int64(stmt, 4, date_j ? (int64_t)date_j->valuedouble : (int64_t)time(NULL));
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
        cJSON_Delete(root);

        cJSON *ok = cJSON_CreateObject();
        cJSON_AddStringToObject(ok, "status", "created");
        send_json(c, 201, ok);
    } else {
        int limit, offset;
        get_pagination(hm, &limit, &offset);
        char sql[512];
        snprintf(sql, sizeof(sql),
            "SELECT q.*, c.display_name as customer_name "
            "FROM quotes q LEFT JOIN contacts c ON q.customer_id = c.id "
            "ORDER BY q.date DESC LIMIT %d OFFSET %d;", limit, offset);
        send_json(c, 200, db_query(app, sql));
    }
}

// Convert Quote → Invoice (one-click)
void route_quote_convert(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
    if (!root) { send_error(c, 400, "Invalid JSON"); return; }

    cJSON *qid_j = cJSON_GetObjectItem(root, "quote_id");
    if (!qid_j) { send_error(c, 400, "quote_id required"); cJSON_Delete(root); return; }

    int qid = qid_j->valueint;
    if (qid <= 0) { send_error(c, 400, "Invalid quote_id"); cJSON_Delete(root); return; }

    // Get quote data — PARAMETERIZED (no SQL injection)
    cJSON *quotes = NULL;
    {
        sqlite3_stmt *qs;
        if (sqlite3_prepare_v2(app->db,
            "SELECT id, customer_id, total FROM quotes WHERE id = ? LIMIT 1;",
            -1, &qs, NULL) == SQLITE_OK) {
            sqlite3_bind_int(qs, 1, qid);
            quotes = cJSON_CreateArray();
            if (sqlite3_step(qs) == SQLITE_ROW) {
                cJSON *row = cJSON_CreateObject();
                cJSON_AddNumberToObject(row, "id",          sqlite3_column_int(qs, 0));
                cJSON_AddNumberToObject(row, "customer_id", sqlite3_column_int(qs, 1));
                cJSON_AddNumberToObject(row, "total",       sqlite3_column_int64(qs, 2));
                cJSON_AddItemToArray(quotes, row);
            }
            sqlite3_finalize(qs);
        }
    }
    if (!quotes || cJSON_GetArraySize(quotes) == 0) {
        send_error(c, 404, "Quote not found");
        cJSON_Delete(quotes); cJSON_Delete(root); return;
    }

    cJSON *q = cJSON_GetArrayItem(quotes, 0);
    int64_t total = (int64_t)cJSON_GetObjectItem(q, "total")->valuedouble;
    int cust_id = (int)cJSON_GetObjectItem(q, "customer_id")->valuedouble;
    int64_t date = (int64_t)time(NULL);

    // Create invoice from quote
    char inv_num[32];
    snprintf(inv_num, sizeof(inv_num), "INV-%ld", (long long)time(NULL) % 100000);

    sqlite3_stmt *stmt;
    const char *ins = "INSERT INTO invoices (invoice_num, customer_id, total, balance_due, date, status, source_quote_id, created_at) "
                      "VALUES (?, ?, ?, ?, ?, 'sent', ?, strftime('%s','now'));";
    if (sqlite3_prepare_v2(app->db, ins, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, inv_num, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, cust_id);
        sqlite3_bind_int64(stmt, 3, total);
        sqlite3_bind_int64(stmt, 4, total);
        sqlite3_bind_int64(stmt, 5, date);
        sqlite3_bind_int(stmt, 6, qid);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    int64_t inv_id = sqlite3_last_insert_rowid(app->db);

    // Update quote status — PARAMETERIZED (no SQL injection)
    {
        sqlite3_stmt *upd;
        if (sqlite3_prepare_v2(app->db,
            "UPDATE quotes SET status = 'converted', converted_invoice_id = ? WHERE id = ?;",
            -1, &upd, NULL) == SQLITE_OK) {
            sqlite3_bind_int64(upd, 1, inv_id);
            sqlite3_bind_int(upd, 2, qid);
            sqlite3_step(upd);
            sqlite3_finalize(upd);
        }
    }

    // Auto-create ledger entry
    cJSON *lines = cJSON_CreateArray();
    cJSON *dr = cJSON_CreateObject();
    cJSON_AddNumberToObject(dr, "account_id", 4);
    cJSON_AddNumberToObject(dr, "debit", total);
    cJSON_AddNumberToObject(dr, "credit", 0);
    cJSON_AddItemToArray(lines, dr);
    cJSON *cr = cJSON_CreateObject();
    cJSON_AddNumberToObject(cr, "account_id", 3);
    cJSON_AddNumberToObject(cr, "debit", 0);
    cJSON_AddNumberToObject(cr, "credit", total);
    cJSON_AddItemToArray(lines, cr);

    char memo[128];
    snprintf(memo, sizeof(memo), "Invoice %s (from Quote)", inv_num);
    ledger_record_entry(app, memo, date, lines);
    cJSON_Delete(lines);
    cJSON_Delete(quotes);
    cJSON_Delete(root);

    cJSON *ok = cJSON_CreateObject();
    cJSON_AddStringToObject(ok, "status", "converted");
    cJSON_AddStringToObject(ok, "invoice_num", inv_num);
    cJSON_AddNumberToObject(ok, "invoice_id", inv_id);
    send_json(c, 200, ok);
}

// ============================================================
// CRM DEALS PIPELINE
// ============================================================
void route_deals(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    if (mg_vcasecmp(&hm->method, "POST") == 0) {
        cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
        if (!root) { send_error(c, 400, "Invalid JSON"); return; }

        cJSON *title = cJSON_GetObjectItem(root, "title");
        cJSON *contact = cJSON_GetObjectItem(root, "contact_id");
        cJSON *amount = cJSON_GetObjectItem(root, "amount");
        cJSON *stage = cJSON_GetObjectItem(root, "stage");
        cJSON *source = cJSON_GetObjectItem(root, "source");
        cJSON *notes = cJSON_GetObjectItem(root, "notes");
        cJSON *prob = cJSON_GetObjectItem(root, "probability");
        cJSON *close_j = cJSON_GetObjectItem(root, "expected_close");

        if (!title || !cJSON_IsString(title)) {
            send_error(c, 400, "Deal title is required");
            cJSON_Delete(root); return;
        }

        sqlite3_stmt *stmt;
        const char *sql = "INSERT INTO deals (title, contact_id, amount, stage, source, notes, probability, expected_close, created_at) "
                          "VALUES (?, ?, ?, ?, ?, ?, ?, ?, strftime('%s','now'));";
        if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, title->valuestring, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 2, (contact && cJSON_IsNumber(contact)) ? contact->valueint : 0);
            sqlite3_bind_int64(stmt, 3, (amount && cJSON_IsNumber(amount)) ? (int64_t)amount->valuedouble : 0);
            sqlite3_bind_text(stmt, 4, (stage && cJSON_IsString(stage)) ? stage->valuestring : "contacted", -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 5, (source && cJSON_IsString(source)) ? source->valuestring : "", -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 6, (notes && cJSON_IsString(notes)) ? notes->valuestring : "", -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 7, (prob && cJSON_IsNumber(prob)) ? prob->valueint : 50);
            sqlite3_bind_int64(stmt, 8, (close_j && cJSON_IsNumber(close_j)) ? (int64_t)close_j->valuedouble : 0);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
        cJSON_Delete(root);

        cJSON *ok = cJSON_CreateObject();
        cJSON_AddStringToObject(ok, "status", "created");
        send_json(c, 201, ok);

    } else if (mg_vcasecmp(&hm->method, "PUT") == 0) {
        // Update deal stage
        cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
        if (!root) { send_error(c, 400, "Invalid JSON"); return; }

        cJSON *id_j = cJSON_GetObjectItem(root, "id");
        cJSON *stage = cJSON_GetObjectItem(root, "stage");
        if (!id_j || !stage) {
            send_error(c, 400, "id and stage required");
            cJSON_Delete(root); return;
        }

        sqlite3_stmt *stmt;
        const char *sql = "UPDATE deals SET stage = ? WHERE id = ?;";
        if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, stage->valuestring, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 2, id_j->valueint);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }

        // If won, set won_date — PARAMETERIZED
        if (strcmp(stage->valuestring, "won") == 0) {
            sqlite3_stmt *wd;
            if (sqlite3_prepare_v2(app->db,
                "UPDATE deals SET won_date = ? WHERE id = ?;",
                -1, &wd, NULL) == SQLITE_OK) {
                sqlite3_bind_int64(wd, 1, (int64_t)time(NULL));
                sqlite3_bind_int(wd, 2, id_j->valueint);
                sqlite3_step(wd);
                sqlite3_finalize(wd);
            }
        }

        cJSON_Delete(root);
        cJSON *ok = cJSON_CreateObject();
        cJSON_AddStringToObject(ok, "status", "updated");
        send_json(c, 200, ok);

    } else {
        // GET — return deals with pipeline summary + pagination
        cJSON *res = cJSON_CreateObject();
        int limit, offset;
        get_pagination(hm, &limit, &offset);

        // Pipeline summary (no pagination — always full)
        cJSON *summary = db_query(app,
            "SELECT stage, COUNT(*) as count, SUM(amount) as total_amount "
            "FROM deals GROUP BY stage;");
        cJSON_AddItemToObject(res, "pipeline", summary);

        // Paginated deals
        char sql[512];
        snprintf(sql, sizeof(sql),
            "SELECT d.*, c.display_name as contact_name "
            "FROM deals d LEFT JOIN contacts c ON d.contact_id = c.id "
            "ORDER BY d.created_at DESC LIMIT %d OFFSET %d;", limit, offset);
        cJSON *deals = db_query(app, sql);
        cJSON_AddItemToObject(res, "deals", deals);

        send_json(c, 200, res);
    }
}

// ============================================================
// INVENTORY MOVEMENTS
// ============================================================
void route_inventory(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    if (mg_vcasecmp(&hm->method, "POST") == 0) {
        cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
        if (!root) { send_error(c, 400, "Invalid JSON"); return; }

        cJSON *item_id = cJSON_GetObjectItem(root, "item_id");
        cJSON *qty = cJSON_GetObjectItem(root, "quantity");
        cJSON *type = cJSON_GetObjectItem(root, "type");

        if (!item_id || !qty) {
            send_error(c, 400, "item_id and quantity required");
            cJSON_Delete(root); return;
        }

        int quantity = qty->valueint;
        const char *mv_type = (type && cJSON_IsString(type)) ? type->valuestring : "adjustment";

        // Begin atomic transaction
        db_execute(app, "BEGIN TRANSACTION;");

        // Insert movement record
        sqlite3_stmt *stmt;
        const char *sql = "INSERT INTO inventory_movements (item_id, type, quantity, notes, created_at) "
                          "VALUES (?, ?, ?, ?, strftime('%s','now'));";
        if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, item_id->valueint);
            sqlite3_bind_text(stmt, 2, mv_type, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 3, quantity);
            cJSON *notes = cJSON_GetObjectItem(root, "notes");
            sqlite3_bind_text(stmt, 4, (notes && cJSON_IsString(notes)) ? notes->valuestring : "", -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }

        // Update item stock — PARAMETERIZED (no SQL injection)
        {
            sqlite3_stmt *upd;
            if (sqlite3_prepare_v2(app->db,
                "UPDATE items SET stock = stock + ? WHERE id = ?;",
                -1, &upd, NULL) == SQLITE_OK) {
                sqlite3_bind_int(upd, 1, quantity);
                sqlite3_bind_int(upd, 2, item_id->valueint);
                sqlite3_step(upd);
                sqlite3_finalize(upd);
            }
        }

        // Check for negative stock — PARAMETERIZED
        bool negative = false;
        {
            sqlite3_stmt *chk;
            if (sqlite3_prepare_v2(app->db,
                "SELECT stock FROM items WHERE id = ?;",
                -1, &chk, NULL) == SQLITE_OK) {
                sqlite3_bind_int(chk, 1, item_id->valueint);
                if (sqlite3_step(chk) == SQLITE_ROW) {
                    if (sqlite3_column_int(chk, 0) < 0) negative = true;
                }
                sqlite3_finalize(chk);
            }
        }

        if (negative) {
            db_execute(app, "ROLLBACK;");
            send_error(c, 400, "Insufficient stock — ghost stock prevented");
        } else {
            db_execute(app, "COMMIT;");
            audit_log_action(app, "STOCK_ADJUST", "item", item_id->valueint, mv_type);
            cJSON *ok = cJSON_CreateObject();
            cJSON_AddStringToObject(ok, "status", "recorded");
            send_json(c, 200, ok);
        }
        cJSON_Delete(root);
    } else {
        // GET — stock summary (items only, not movements — no pagination needed for small dataset)
        int limit, offset;
        get_pagination(hm, &limit, &offset);
        char sql[512];
        snprintf(sql, sizeof(sql),
            "SELECT i.id, i.name, i.sku, i.stock, i.price, i.reorder_level, "
            "i.warehouse_id, w.name as warehouse_name "
            "FROM items i LEFT JOIN warehouses w ON i.warehouse_id = w.id "
            "WHERE i.is_active = 1 ORDER BY i.name LIMIT %d OFFSET %d;", limit, offset);
        send_json(c, 200, db_query(app, sql));
    }
}

// ============================================================
// CREDIT NOTES — Sales Returns / Adjustments
// ============================================================
void route_credit_notes(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    if (mg_vcasecmp(&hm->method, "POST") == 0) {
        cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
        if (!root) { send_error(c, 400, "Invalid JSON"); return; }

        cJSON *inv_j = cJSON_GetObjectItem(root, "invoice_id");
        cJSON *cust_j = cJSON_GetObjectItem(root, "customer_id");
        cJSON *amt_j = cJSON_GetObjectItem(root, "amount");
        cJSON *reason_j = cJSON_GetObjectItem(root, "reason");

        if (!cust_j || !amt_j) {
            send_error(c, 400, "customer_id and amount required");
            cJSON_Delete(root); return;
        }

        int64_t amount = (int64_t)(amt_j->valuedouble);
        cJSON *date_j = cJSON_GetObjectItem(root, "date");
        int64_t date = date_j && cJSON_IsNumber(date_j) ? (int64_t)date_j->valuedouble : (int64_t)time(NULL);

        // Generate CN number
        char cn_num[32];
        snprintf(cn_num, sizeof(cn_num), "CN-%ld", (long long)time(NULL) % 100000);

        sqlite3_stmt *stmt;
        // Schema: cn_num, customer_id, invoice_id, amount, reason, date, created_at
        const char *sql = "INSERT INTO credit_notes (cn_num, customer_id, invoice_id, amount, reason, date, created_at) "
                          "VALUES (?, ?, ?, ?, ?, ?, strftime('%s','now'));";
        if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, cn_num, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 2, cust_j->valueint);
            sqlite3_bind_int(stmt, 3, (inv_j && cJSON_IsNumber(inv_j)) ? inv_j->valueint : 0);
            sqlite3_bind_int64(stmt, 4, amount);
            sqlite3_bind_text(stmt, 5, (reason_j && cJSON_IsString(reason_j)) ? reason_j->valuestring : "", -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(stmt, 6, date);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }

        // Reversal Ledger: Debit Sales(3), Credit AR(4)
        cJSON *lines = cJSON_CreateArray();
        cJSON *dr = cJSON_CreateObject();
        cJSON_AddNumberToObject(dr, "account_id", 3);
        cJSON_AddNumberToObject(dr, "debit", amount);
        cJSON_AddNumberToObject(dr, "credit", 0);
        cJSON_AddItemToArray(lines, dr);
        cJSON *cr = cJSON_CreateObject();
        cJSON_AddNumberToObject(cr, "account_id", 4);
        cJSON_AddNumberToObject(cr, "debit", 0);
        cJSON_AddNumberToObject(cr, "credit", amount);
        cJSON_AddItemToArray(lines, cr);

        char memo[128];
        snprintf(memo, sizeof(memo), "Credit Note %s", cn_num);
        ledger_record_entry(app, memo, date, lines);
        cJSON_Delete(lines);

        audit_log_action(app, "CREDIT_NOTE_ISSUED", "credit_note", 0, cn_num);
        cJSON_Delete(root);

        cJSON *ok = cJSON_CreateObject();
        cJSON_AddStringToObject(ok, "status", "created");
        cJSON_AddStringToObject(ok, "credit_note_num", cn_num);
        send_json(c, 201, ok);
    } else {
        int limit, offset;
        get_pagination(hm, &limit, &offset);
        char sql[512];
        snprintf(sql, sizeof(sql),
            "SELECT cn.*, c.display_name as customer_name "
            "FROM credit_notes cn LEFT JOIN contacts c ON cn.customer_id = c.id "
            "ORDER BY cn.date DESC LIMIT %d OFFSET %d;", limit, offset);
        send_json(c, 200, db_query(app, sql));
    }
}

// ============================================================
// VENDOR CREDITS — Debit Notes from Vendors
// ============================================================
void route_vendor_credits(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    if (mg_vcasecmp(&hm->method, "POST") == 0) {
        cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
        if (!root) { send_error(c, 400, "Invalid JSON"); return; }

        cJSON *vendor_j = cJSON_GetObjectItem(root, "vendor_id");
        cJSON *bill_j = cJSON_GetObjectItem(root, "bill_id");
        cJSON *amt_j = cJSON_GetObjectItem(root, "amount");
        cJSON *reason_j = cJSON_GetObjectItem(root, "reason");

        if (!vendor_j || !amt_j) {
            send_error(c, 400, "vendor_id and amount required");
            cJSON_Delete(root); return;
        }

        int64_t amount = (int64_t)(amt_j->valuedouble);
        cJSON *date_j2 = cJSON_GetObjectItem(root, "date");
        int64_t vc_date = date_j2 && cJSON_IsNumber(date_j2) ? (int64_t)date_j2->valuedouble : (int64_t)time(NULL);

        char vc_num[32];
        snprintf(vc_num, sizeof(vc_num), "VC-%ld", (long long)time(NULL) % 100000);

        sqlite3_stmt *stmt;
        // Schema: vc_num, vendor_id, bill_id, amount, reason, date, created_at
        const char *sql = "INSERT INTO vendor_credits (vc_num, vendor_id, bill_id, amount, reason, date, created_at) "
                          "VALUES (?, ?, ?, ?, ?, ?, strftime('%s','now'));";
        if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, vc_num, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 2, vendor_j->valueint);
            sqlite3_bind_int(stmt, 3, (bill_j && cJSON_IsNumber(bill_j)) ? bill_j->valueint : 0);
            sqlite3_bind_int64(stmt, 4, amount);
            sqlite3_bind_text(stmt, 5, (reason_j && cJSON_IsString(reason_j)) ? reason_j->valuestring : "", -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(stmt, 6, vc_date);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }

        // Reversal Ledger: Debit AP(5), Credit Expense(8)
        cJSON *lines = cJSON_CreateArray();
        cJSON *dr = cJSON_CreateObject();
        cJSON_AddNumberToObject(dr, "account_id", 5);
        cJSON_AddNumberToObject(dr, "debit", amount);
        cJSON_AddNumberToObject(dr, "credit", 0);
        cJSON_AddItemToArray(lines, dr);
        cJSON *cr = cJSON_CreateObject();
        cJSON_AddNumberToObject(cr, "account_id", 8);
        cJSON_AddNumberToObject(cr, "debit", 0);
        cJSON_AddNumberToObject(cr, "credit", amount);
        cJSON_AddItemToArray(lines, cr);

        char memo[128];
        snprintf(memo, sizeof(memo), "Vendor Credit %s", vc_num);
        ledger_record_entry(app, memo, (int64_t)time(NULL), lines);
        cJSON_Delete(lines);

        audit_log_action(app, "VENDOR_CREDIT_ISSUED", "vendor_credit", 0, vc_num);
        cJSON_Delete(root);

        cJSON *ok = cJSON_CreateObject();
        cJSON_AddStringToObject(ok, "status", "created");
        cJSON_AddStringToObject(ok, "vendor_credit_num", vc_num);
        send_json(c, 201, ok);
    } else {
        int limit, offset;
        get_pagination(hm, &limit, &offset);
        char sql[512];
        snprintf(sql, sizeof(sql),
            "SELECT vc.*, c.display_name as vendor_name "
            "FROM vendor_credits vc LEFT JOIN contacts c ON vc.vendor_id = c.id "
            "ORDER BY vc.date DESC LIMIT %d OFFSET %d;", limit, offset);
        send_json(c, 200, db_query(app, sql));
    }
}

// ============================================================
// PURCHASE ORDERS
// ============================================================
void route_purchase_orders(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    if (mg_vcasecmp(&hm->method, "POST") == 0) {
        cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
        if (!root) { send_error(c, 400, "Invalid JSON"); return; }

        cJSON *num = cJSON_GetObjectItem(root, "po_num");
        cJSON *vendor = cJSON_GetObjectItem(root, "vendor_id");
        cJSON *total_j = cJSON_GetObjectItem(root, "total");
        cJSON *date_j = cJSON_GetObjectItem(root, "date");
        cJSON *delivery_j = cJSON_GetObjectItem(root, "delivery_date");

        if (!num || !vendor || !total_j) {
            send_error(c, 400, "po_num, vendor_id, and total required");
            cJSON_Delete(root); return;
        }

        sqlite3_stmt *stmt;
        // Schema: po_num, vendor_id, total, date, expected_date (NOT delivery_date), status, created_at
        const char *sql = "INSERT INTO purchase_orders (po_num, vendor_id, total, date, expected_date, status, created_at) "
                          "VALUES (?, ?, ?, ?, ?, 'draft', strftime('%s','now'));";
        if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, num->valuestring, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 2, vendor->valueint);
            sqlite3_bind_int64(stmt, 3, (int64_t)total_j->valuedouble);
            sqlite3_bind_int64(stmt, 4, date_j ? (int64_t)date_j->valuedouble : (int64_t)time(NULL));
            sqlite3_bind_int64(stmt, 5, delivery_j ? (int64_t)delivery_j->valuedouble : 0);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }

        audit_log_action(app, "PO_CREATED", "purchase_order", 0, num->valuestring);
        cJSON_Delete(root);

        cJSON *ok = cJSON_CreateObject();
        cJSON_AddStringToObject(ok, "status", "created");
        send_json(c, 201, ok);
    } else {
        int limit, offset;
        get_pagination(hm, &limit, &offset);
        char sql[512];
        snprintf(sql, sizeof(sql),
            "SELECT po.*, c.display_name as vendor_name "
            "FROM purchase_orders po LEFT JOIN contacts c ON po.vendor_id = c.id "
            "ORDER BY po.date DESC LIMIT %d OFFSET %d;", limit, offset);
        send_json(c, 200, db_query(app, sql));
    }
}

// ============================================================
// CONTACT EDIT + DELETE (P2-18)
// ============================================================
void route_contact_update(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id) {
    if (mg_vcasecmp(&hm->method, "PUT") == 0) {
        cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
        if (!root) { send_error(c, 400, "Invalid JSON"); return; }

        // Build dynamic UPDATE
        sqlite3_stmt *stmt;
        const char *sql = "UPDATE contacts SET "
                          "display_name = COALESCE(?, display_name), "
                          "company_name = COALESCE(?, company_name), "
                          "email = COALESCE(?, email), "
                          "phone = COALESCE(?, phone), "
                          "gstin = COALESCE(?, gstin), "
                          "type = COALESCE(?, type) "
                          "WHERE id = ?;";
        if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            cJSON *f;
            f = cJSON_GetObjectItem(root, "display_name");
            (f && cJSON_IsString(f)) ? sqlite3_bind_text(stmt, 1, f->valuestring, -1, SQLITE_TRANSIENT) : sqlite3_bind_null(stmt, 1);
            f = cJSON_GetObjectItem(root, "company_name");
            (f && cJSON_IsString(f)) ? sqlite3_bind_text(stmt, 2, f->valuestring, -1, SQLITE_TRANSIENT) : sqlite3_bind_null(stmt, 2);
            f = cJSON_GetObjectItem(root, "email");
            (f && cJSON_IsString(f)) ? sqlite3_bind_text(stmt, 3, f->valuestring, -1, SQLITE_TRANSIENT) : sqlite3_bind_null(stmt, 3);
            f = cJSON_GetObjectItem(root, "phone");
            (f && cJSON_IsString(f)) ? sqlite3_bind_text(stmt, 4, f->valuestring, -1, SQLITE_TRANSIENT) : sqlite3_bind_null(stmt, 4);
            f = cJSON_GetObjectItem(root, "gstin");
            (f && cJSON_IsString(f)) ? sqlite3_bind_text(stmt, 5, f->valuestring, -1, SQLITE_TRANSIENT) : sqlite3_bind_null(stmt, 5);
            f = cJSON_GetObjectItem(root, "type");
            (f && cJSON_IsString(f)) ? sqlite3_bind_text(stmt, 6, f->valuestring, -1, SQLITE_TRANSIENT) : sqlite3_bind_null(stmt, 6);
            sqlite3_bind_int(stmt, 7, id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }

        audit_log_action(app, "CONTACT_UPDATED", "contact", id, "Fields modified");
        cJSON_Delete(root);

        cJSON *ok = cJSON_CreateObject();
        cJSON_AddStringToObject(ok, "status", "updated");
        send_json(c, 200, ok);

    } else if (mg_vcasecmp(&hm->method, "DELETE") == 0) {
        // Soft delete — set is_active = 0
        sqlite3_stmt *stmt;
        const char *sql = "UPDATE contacts SET is_active = 0 WHERE id = ?;";
        if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }

        audit_log_action(app, "CONTACT_DELETED", "contact", id, "Soft deleted");

        cJSON *ok = cJSON_CreateObject();
        cJSON_AddStringToObject(ok, "status", "deleted");
        send_json(c, 200, ok);
    } else {
        // GET single contact
        sqlite3_stmt *stmt;
        const char *sql = "SELECT * FROM contacts WHERE id = ?;";
        if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, id);
        }
        cJSON *list = db_query(app, "SELECT * FROM contacts WHERE id = ?;");
        send_json(c, 200, list);
    }
}

// ============================================================
// INVOICE VOID (P2-19) — Creates Reversal Journal Entry
// ============================================================
void route_invoice_void(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id) {
    // Get invoice data
    sqlite3_stmt *stmt;
    const char *sql = "SELECT total, customer_id, invoice_num FROM invoices WHERE id = ? AND status != 'void';";
    if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        send_error(c, 500, "DB error"); return;
    }

    sqlite3_bind_int(stmt, 1, id);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        send_error(c, 404, "Invoice not found or already voided");
        return;
    }

    int64_t total = sqlite3_column_int64(stmt, 0);
    const char *inv_num = (const char*)sqlite3_column_text(stmt, 2);
    char inv_num_copy[64];
    strncpy(inv_num_copy, inv_num ? inv_num : "", sizeof(inv_num_copy));
    sqlite3_finalize(stmt);

    // 1. Set invoice status to void
    const char *void_sql = "UPDATE invoices SET status = 'void', balance_due = 0 WHERE id = ?;";
    if (sqlite3_prepare_v2(app->db, void_sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // 2. Create reversal journal entry: Debit Sales(3), Credit AR(4)
    cJSON *lines = cJSON_CreateArray();
    cJSON *dr = cJSON_CreateObject();
    cJSON_AddNumberToObject(dr, "account_id", 3);
    cJSON_AddNumberToObject(dr, "debit", (double)total);
    cJSON_AddNumberToObject(dr, "credit", 0);
    cJSON_AddItemToArray(lines, dr);
    cJSON *cr = cJSON_CreateObject();
    cJSON_AddNumberToObject(cr, "account_id", 4);
    cJSON_AddNumberToObject(cr, "debit", 0);
    cJSON_AddNumberToObject(cr, "credit", (double)total);
    cJSON_AddItemToArray(lines, cr);

    char memo[128];
    snprintf(memo, sizeof(memo), "VOID: Invoice %s (reversal)", inv_num_copy);
    ledger_record_entry(app, memo, (int64_t)time(NULL), lines);
    cJSON_Delete(lines);

    audit_log_action(app, "INVOICE_VOIDED", "invoice", id, inv_num_copy);

    cJSON *ok = cJSON_CreateObject();
    cJSON_AddStringToObject(ok, "status", "voided");
    cJSON_AddStringToObject(ok, "message", "Invoice voided. Reversal journal entry created.");
    send_json(c, 200, ok);
}

// ============================================================
// INVOICE UPDATE — PUT /api/invoices/:id
// ============================================================
void route_invoice_update(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id) {
    if (mg_vcasecmp(&hm->method, "PUT") != 0) { send_error(c, 405, "Method not allowed"); return; }

    cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
    if (!root) { send_error(c, 400, "Invalid JSON"); return; }

    sqlite3_stmt *stmt;
    const char *sql = "UPDATE invoices SET "
        "notes = COALESCE(?, notes), "
        "due_date = COALESCE(?, due_date), "
        "status = COALESCE(?, status) "
        "WHERE id = ? AND status != 'void';";

    if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        cJSON *f;
        f = cJSON_GetObjectItem(root, "notes");
        (f && cJSON_IsString(f)) ? sqlite3_bind_text(stmt, 1, f->valuestring, -1, SQLITE_TRANSIENT)
                                 : sqlite3_bind_null(stmt, 1);
        f = cJSON_GetObjectItem(root, "due_date");
        (f && cJSON_IsNumber(f)) ? sqlite3_bind_int64(stmt, 2, (int64_t)f->valuedouble)
                                 : sqlite3_bind_null(stmt, 2);
        f = cJSON_GetObjectItem(root, "status");
        (f && cJSON_IsString(f)) ? sqlite3_bind_text(stmt, 3, f->valuestring, -1, SQLITE_TRANSIENT)
                                 : sqlite3_bind_null(stmt, 3);
        sqlite3_bind_int(stmt, 4, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    audit_log_action(app, "INVOICE_UPDATED", "invoice", id, "Fields modified");
    cJSON_Delete(root);

    cJSON *ok = cJSON_CreateObject();
    cJSON_AddStringToObject(ok, "status", "updated");
    send_json(c, 200, ok);
}

// ============================================================
// BILL UPDATE — PUT /api/bills/:id
// BILL DELETE — DELETE /api/bills/:id
// ============================================================
void route_bill_update(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id) {
    if (mg_vcasecmp(&hm->method, "PUT") != 0) { send_error(c, 405, "Method not allowed"); return; }

    cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
    if (!root) { send_error(c, 400, "Invalid JSON"); return; }

    sqlite3_stmt *stmt;
    const char *sql = "UPDATE bills SET "
        "notes = COALESCE(?, notes), "
        "due_date = COALESCE(?, due_date), "
        "status = COALESCE(?, status) "
        "WHERE id = ?;";

    if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        cJSON *f;
        f = cJSON_GetObjectItem(root, "notes");
        (f && cJSON_IsString(f)) ? sqlite3_bind_text(stmt, 1, f->valuestring, -1, SQLITE_TRANSIENT)
                                 : sqlite3_bind_null(stmt, 1);
        f = cJSON_GetObjectItem(root, "due_date");
        (f && cJSON_IsNumber(f)) ? sqlite3_bind_int64(stmt, 2, (int64_t)f->valuedouble)
                                 : sqlite3_bind_null(stmt, 2);
        f = cJSON_GetObjectItem(root, "status");
        (f && cJSON_IsString(f)) ? sqlite3_bind_text(stmt, 3, f->valuestring, -1, SQLITE_TRANSIENT)
                                 : sqlite3_bind_null(stmt, 3);
        sqlite3_bind_int(stmt, 4, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    audit_log_action(app, "BILL_UPDATED", "bill", id, "Fields modified");
    cJSON_Delete(root);

    cJSON *ok = cJSON_CreateObject();
    cJSON_AddStringToObject(ok, "status", "updated");
    send_json(c, 200, ok);
}

void route_bill_delete(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id) {
    if (mg_vcasecmp(&hm->method, "DELETE") != 0) { send_error(c, 405, "Method not allowed"); return; }

    sqlite3_stmt *stmt;
    const char *sql = "UPDATE bills SET status = 'void' WHERE id = ?;";
    if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    audit_log_action(app, "BILL_VOIDED", "bill", id, "Voided by user");

    cJSON *ok = cJSON_CreateObject();
    cJSON_AddStringToObject(ok, "status", "voided");
    send_json(c, 200, ok);
}

// ============================================================
// ITEM UPDATE — PUT /api/items/:id
// ITEM DELETE — DELETE /api/items/:id
// ============================================================
void route_item_update(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id) {
    if (mg_vcasecmp(&hm->method, "PUT") != 0) { send_error(c, 405, "Method not allowed"); return; }

    cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
    if (!root) { send_error(c, 400, "Invalid JSON"); return; }

    sqlite3_stmt *stmt;
    const char *sql = "UPDATE items SET "
        "name       = COALESCE(?, name), "
        "sku        = COALESCE(?, sku), "
        "price      = COALESCE(?, price), "
        "cost_price = COALESCE(?, cost_price), "
        "tax_rate   = COALESCE(?, tax_rate), "
        "stock      = COALESCE(?, stock), "
        "unit       = COALESCE(?, unit) "
        "WHERE id = ?;";

    if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        cJSON *f;
        f = cJSON_GetObjectItem(root, "name");
        (f && cJSON_IsString(f)) ? sqlite3_bind_text(stmt, 1, f->valuestring, -1, SQLITE_TRANSIENT)
                                 : sqlite3_bind_null(stmt, 1);
        f = cJSON_GetObjectItem(root, "sku");
        (f && cJSON_IsString(f)) ? sqlite3_bind_text(stmt, 2, f->valuestring, -1, SQLITE_TRANSIENT)
                                 : sqlite3_bind_null(stmt, 2);
        f = cJSON_GetObjectItem(root, "price");
        (f && cJSON_IsNumber(f)) ? sqlite3_bind_int64(stmt, 3, (int64_t)f->valuedouble)
                                 : sqlite3_bind_null(stmt, 3);
        f = cJSON_GetObjectItem(root, "cost_price");
        (f && cJSON_IsNumber(f)) ? sqlite3_bind_int64(stmt, 4, (int64_t)f->valuedouble)
                                 : sqlite3_bind_null(stmt, 4);
        f = cJSON_GetObjectItem(root, "tax_rate");
        (f && cJSON_IsNumber(f)) ? sqlite3_bind_double(stmt, 5, f->valuedouble)
                                 : sqlite3_bind_null(stmt, 5);
        f = cJSON_GetObjectItem(root, "stock");
        (f && cJSON_IsNumber(f)) ? sqlite3_bind_int(stmt, 6, f->valueint)
                                 : sqlite3_bind_null(stmt, 6);
        f = cJSON_GetObjectItem(root, "unit");
        (f && cJSON_IsString(f)) ? sqlite3_bind_text(stmt, 7, f->valuestring, -1, SQLITE_TRANSIENT)
                                 : sqlite3_bind_null(stmt, 7);
        sqlite3_bind_int(stmt, 8, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    audit_log_action(app, "ITEM_UPDATED", "item", id, "Fields modified");
    cJSON_Delete(root);

    cJSON *ok = cJSON_CreateObject();
    cJSON_AddStringToObject(ok, "status", "updated");
    send_json(c, 200, ok);
}

void route_item_delete(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id) {
    if (mg_vcasecmp(&hm->method, "DELETE") != 0) { send_error(c, 405, "Method not allowed"); return; }

    sqlite3_stmt *stmt;
    const char *sql = "UPDATE items SET is_active = 0 WHERE id = ?;";
    if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    audit_log_action(app, "ITEM_DELETED", "item", id, "Soft deleted");

    cJSON *ok = cJSON_CreateObject();
    cJSON_AddStringToObject(ok, "status", "deleted");
    send_json(c, 200, ok);
}

// ============================================================
// QUOTE UPDATE — PUT /api/quotes/:id
// QUOTE DELETE — DELETE /api/quotes/:id
// ============================================================
void route_quote_update(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id) {
    if (mg_vcasecmp(&hm->method, "PUT") != 0) { send_error(c, 405, "Method not allowed"); return; }

    cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
    if (!root) { send_error(c, 400, "Invalid JSON"); return; }

    sqlite3_stmt *stmt;
    const char *sql = "UPDATE quotes SET "
        "status      = COALESCE(?, status), "
        "expiry_date = COALESCE(?, expiry_date), "
        "notes       = COALESCE(?, notes) "
        "WHERE id = ?;";

    if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        cJSON *f;
        f = cJSON_GetObjectItem(root, "status");
        (f && cJSON_IsString(f)) ? sqlite3_bind_text(stmt, 1, f->valuestring, -1, SQLITE_TRANSIENT)
                                 : sqlite3_bind_null(stmt, 1);
        f = cJSON_GetObjectItem(root, "expiry_date");
        (f && cJSON_IsNumber(f)) ? sqlite3_bind_int64(stmt, 2, (int64_t)f->valuedouble)
                                 : sqlite3_bind_null(stmt, 2);
        f = cJSON_GetObjectItem(root, "notes");
        (f && cJSON_IsString(f)) ? sqlite3_bind_text(stmt, 3, f->valuestring, -1, SQLITE_TRANSIENT)
                                 : sqlite3_bind_null(stmt, 3);
        sqlite3_bind_int(stmt, 4, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    audit_log_action(app, "QUOTE_UPDATED", "quote", id, "Fields modified");
    cJSON_Delete(root);

    cJSON *ok = cJSON_CreateObject();
    cJSON_AddStringToObject(ok, "status", "updated");
    send_json(c, 200, ok);
}

void route_quote_delete(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id) {
    if (mg_vcasecmp(&hm->method, "DELETE") != 0) { send_error(c, 405, "Method not allowed"); return; }

    sqlite3_stmt *stmt;
    const char *sql = "UPDATE quotes SET status = 'declined' WHERE id = ?;";
    if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    audit_log_action(app, "QUOTE_DELETED", "quote", id, "Declined/deleted");

    cJSON *ok = cJSON_CreateObject();
    cJSON_AddStringToObject(ok, "status", "deleted");
    send_json(c, 200, ok);
}

// ============================================================
// GLOBAL SEARCH — GET /api/search?q=XYZ&mode=like
// ============================================================
void route_search(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    if (mg_vcasecmp(&hm->method, "GET") != 0) { send_error(c, 405, "Method not allowed"); return; }

    char qbuf[128] = "";
    char mode[32] = "like";
    mg_http_get_var(&hm->query, "q", qbuf, sizeof(qbuf));
    mg_http_get_var(&hm->query, "mode", mode, sizeof(mode));

    if (strlen(qbuf) == 0) {
        send_json(c, 200, cJSON_CreateArray());
        return;
    }

    cJSON *results = cJSON_CreateArray();

    // Prepare LIKE pattern
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "%%%s%%", qbuf);

    sqlite3_stmt *stmt;
    
    // 1. Search Contacts
    const char *sql_contacts = "SELECT id, display_name, email, phone FROM contacts WHERE is_active=1 AND (display_name LIKE ? OR email LIKE ? OR phone LIKE ?) LIMIT 5;";
    if (sqlite3_prepare_v2(app->db, sql_contacts, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, pattern, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, pattern, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, pattern, -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "type", "Contact");
            cJSON_AddNumberToObject(item, "id", sqlite3_column_int(stmt, 0));
            cJSON_AddStringToObject(item, "title", (const char*)sqlite3_column_text(stmt, 1));
            
            char snippet[256];
            snprintf(snippet, sizeof(snippet), "%s | %s", 
                     sqlite3_column_text(stmt, 2) ? (const char*)sqlite3_column_text(stmt, 2) : "",
                     sqlite3_column_text(stmt, 3) ? (const char*)sqlite3_column_text(stmt, 3) : "");
            cJSON_AddStringToObject(item, "snippet", snippet);
            
            char url[64];
            snprintf(url, sizeof(url), "#/contacts");
            cJSON_AddStringToObject(item, "url", url);
            cJSON_AddItemToArray(results, item);
        }
        sqlite3_finalize(stmt);
    }

    // 2. Search Invoices
    const char *sql_invoices = "SELECT i.id, i.invoice_num, i.total, c.display_name FROM invoices i LEFT JOIN contacts c ON i.customer_id = c.id WHERE i.invoice_num LIKE ? LIMIT 5;";
    if (sqlite3_prepare_v2(app->db, sql_invoices, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, pattern, -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "type", "Invoice");
            cJSON_AddNumberToObject(item, "id", sqlite3_column_int(stmt, 0));
            cJSON_AddStringToObject(item, "title", (const char*)sqlite3_column_text(stmt, 1));
            
            char snippet[256];
            snprintf(snippet, sizeof(snippet), "Customer: %s | Amount: ₹%.2f", 
                     sqlite3_column_text(stmt, 3) ? (const char*)sqlite3_column_text(stmt, 3) : "Unknown",
                     sqlite3_column_double(stmt, 2) / 100.0);
            cJSON_AddStringToObject(item, "snippet", snippet);
            
            char url[64];
            snprintf(url, sizeof(url), "#/invoices/%d", sqlite3_column_int(stmt, 0));
            cJSON_AddStringToObject(item, "url", url);
            cJSON_AddItemToArray(results, item);
        }
        sqlite3_finalize(stmt);
    }

    // 3. Search Items
    const char *sql_items = "SELECT id, name, sku, price FROM items WHERE is_active=1 AND (name LIKE ? OR sku LIKE ?) LIMIT 5;";
    if (sqlite3_prepare_v2(app->db, sql_items, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, pattern, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, pattern, -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "type", "Item");
            cJSON_AddNumberToObject(item, "id", sqlite3_column_int(stmt, 0));
            cJSON_AddStringToObject(item, "title", (const char*)sqlite3_column_text(stmt, 1));
            
            char snippet[256];
            snprintf(snippet, sizeof(snippet), "SKU: %s | Price: ₹%.2f", 
                     sqlite3_column_text(stmt, 2) ? (const char*)sqlite3_column_text(stmt, 2) : "N/A",
                     sqlite3_column_double(stmt, 3) / 100.0);
            cJSON_AddStringToObject(item, "snippet", snippet);
            
            char url[64];
            snprintf(url, sizeof(url), "#/items");
            cJSON_AddStringToObject(item, "url", url);
            cJSON_AddItemToArray(results, item);
        }
        sqlite3_finalize(stmt);
    }

    send_json(c, 200, results);
}

// ============================================================
// REMAINING CRUD (Deals, Credit Notes, Vendor Credits, Purchase Orders)
// ============================================================

// --- Deals CRUD ---
void route_deal_update(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id) {
    if (mg_vcasecmp(&hm->method, "PUT") != 0) { send_error(c, 405, "Method not allowed"); return; }
    cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
    if (!root) { send_error(c, 400, "Invalid JSON"); return; }

    sqlite3_stmt *stmt;
    const char *sql = "UPDATE deals SET stage = COALESCE(?, stage), amount = COALESCE(?, amount), notes = COALESCE(?, notes) WHERE id = ?;";
    if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        cJSON *f = cJSON_GetObjectItem(root, "stage");
        (f && cJSON_IsString(f)) ? sqlite3_bind_text(stmt, 1, f->valuestring, -1, SQLITE_TRANSIENT) : sqlite3_bind_null(stmt, 1);
        f = cJSON_GetObjectItem(root, "amount");
        (f && cJSON_IsNumber(f)) ? sqlite3_bind_int64(stmt, 2, (int64_t)f->valuedouble) : sqlite3_bind_null(stmt, 2);
        f = cJSON_GetObjectItem(root, "notes");
        (f && cJSON_IsString(f)) ? sqlite3_bind_text(stmt, 3, f->valuestring, -1, SQLITE_TRANSIENT) : sqlite3_bind_null(stmt, 3);
        sqlite3_bind_int(stmt, 4, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    audit_log_action(app, "DEAL_UPDATED", "deal", id, "Fields modified");
    cJSON_Delete(root);
    cJSON *ok = cJSON_CreateObject();
    cJSON_AddStringToObject(ok, "status", "updated");
    send_json(c, 200, ok);
}

void route_deal_delete(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id) {
    if (mg_vcasecmp(&hm->method, "DELETE") != 0) { send_error(c, 405, "Method not allowed"); return; }
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE deals SET stage = 'lost' WHERE id = ?;";
    if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    audit_log_action(app, "DEAL_DELETED", "deal", id, "Soft deleted (lost)");
    cJSON *ok = cJSON_CreateObject();
    cJSON_AddStringToObject(ok, "status", "deleted");
    send_json(c, 200, ok);
}

// --- Credit Note CRUD ---
void route_credit_note_update(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id) {
    if (mg_vcasecmp(&hm->method, "PUT") != 0) { send_error(c, 405, "Method not allowed"); return; }
    cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
    if (!root) { send_error(c, 400, "Invalid JSON"); return; }

    sqlite3_stmt *stmt;
    const char *sql = "UPDATE credit_notes SET reason = COALESCE(?, reason) WHERE id = ?;";
    if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        cJSON *f = cJSON_GetObjectItem(root, "reason");
        (f && cJSON_IsString(f)) ? sqlite3_bind_text(stmt, 1, f->valuestring, -1, SQLITE_TRANSIENT) : sqlite3_bind_null(stmt, 1);
        sqlite3_bind_int(stmt, 2, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    audit_log_action(app, "CREDIT_NOTE_UPDATED", "credit_note", id, "Fields modified");
    cJSON_Delete(root);
    cJSON *ok = cJSON_CreateObject();
    cJSON_AddStringToObject(ok, "status", "updated");
    send_json(c, 200, ok);
}

void route_credit_note_delete(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id) {
    if (mg_vcasecmp(&hm->method, "DELETE") != 0) { send_error(c, 405, "Method not allowed"); return; }
    send_error(c, 400, "Credit Notes cannot be deleted, they are permanent ledger entries.");
}

// --- Vendor Credit CRUD ---
void route_vendor_credit_update(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id) {
    if (mg_vcasecmp(&hm->method, "PUT") != 0) { send_error(c, 405, "Method not allowed"); return; }
    cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
    if (!root) { send_error(c, 400, "Invalid JSON"); return; }

    sqlite3_stmt *stmt;
    const char *sql = "UPDATE vendor_credits SET reason = COALESCE(?, reason) WHERE id = ?;";
    if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        cJSON *f = cJSON_GetObjectItem(root, "reason");
        (f && cJSON_IsString(f)) ? sqlite3_bind_text(stmt, 1, f->valuestring, -1, SQLITE_TRANSIENT) : sqlite3_bind_null(stmt, 1);
        sqlite3_bind_int(stmt, 2, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    audit_log_action(app, "VENDOR_CREDIT_UPDATED", "vendor_credit", id, "Fields modified");
    cJSON_Delete(root);
    cJSON *ok = cJSON_CreateObject();
    cJSON_AddStringToObject(ok, "status", "updated");
    send_json(c, 200, ok);
}

void route_vendor_credit_delete(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id) {
    if (mg_vcasecmp(&hm->method, "DELETE") != 0) { send_error(c, 405, "Method not allowed"); return; }
    send_error(c, 400, "Vendor Credits cannot be deleted, they are permanent ledger entries.");
}

// --- Purchase Order CRUD ---
void route_purchase_order_update(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id) {
    if (mg_vcasecmp(&hm->method, "PUT") != 0) { send_error(c, 405, "Method not allowed"); return; }
    cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
    if (!root) { send_error(c, 400, "Invalid JSON"); return; }

    sqlite3_stmt *stmt;
    const char *sql = "UPDATE purchase_orders SET status = COALESCE(?, status) WHERE id = ?;";
    if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        cJSON *f = cJSON_GetObjectItem(root, "status");
        (f && cJSON_IsString(f)) ? sqlite3_bind_text(stmt, 1, f->valuestring, -1, SQLITE_TRANSIENT) : sqlite3_bind_null(stmt, 1);
        sqlite3_bind_int(stmt, 2, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    audit_log_action(app, "PURCHASE_ORDER_UPDATED", "purchase_order", id, "Fields modified");
    cJSON_Delete(root);
    cJSON *ok = cJSON_CreateObject();
    cJSON_AddStringToObject(ok, "status", "updated");
    send_json(c, 200, ok);
}

void route_purchase_order_delete(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id) {
    if (mg_vcasecmp(&hm->method, "DELETE") != 0) { send_error(c, 405, "Method not allowed"); return; }
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE purchase_orders SET status = 'void' WHERE id = ?;";
    if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    audit_log_action(app, "PURCHASE_ORDER_DELETED", "purchase_order", id, "Soft deleted (voided)");
    cJSON *ok = cJSON_CreateObject();
    cJSON_AddStringToObject(ok, "status", "deleted");
    send_json(c, 200, ok);
}

// ============================================================
// ACTIVITIES — CRM Follow-up & Activity Log
// ============================================================
void route_activities(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    if (mg_vcasecmp(&hm->method, "POST") == 0) {
        cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
        if (!root) { send_error(c, 400, "Invalid JSON"); return; }

        cJSON *type_j        = cJSON_GetObjectItem(root, "type");
        cJSON *contact_id_j  = cJSON_GetObjectItem(root, "contact_id");
        cJSON *deal_id_j     = cJSON_GetObjectItem(root, "deal_id");
        cJSON *subject_j     = cJSON_GetObjectItem(root, "subject");
        cJSON *description_j = cJSON_GetObjectItem(root, "description");
        cJSON *date_j        = cJSON_GetObjectItem(root, "date");

        const char *sql = "INSERT INTO activities "
            "(type, contact_id, deal_id, subject, description, date, completed, created_at) "
            "VALUES (?,?,?,?,?,?,0,?);";
        sqlite3_stmt *stmt;
        long long now = (long long)time(NULL);
        if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt,  1, (type_j && cJSON_IsString(type_j)) ? type_j->valuestring : "note", -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt,   2, (contact_id_j && cJSON_IsNumber(contact_id_j)) ? contact_id_j->valueint : 0);
            sqlite3_bind_int(stmt,   3, (deal_id_j && cJSON_IsNumber(deal_id_j)) ? deal_id_j->valueint : 0);
            sqlite3_bind_text(stmt,  4, (subject_j && cJSON_IsString(subject_j)) ? subject_j->valuestring : "", -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt,  5, (description_j && cJSON_IsString(description_j)) ? description_j->valuestring : "", -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(stmt, 6, (date_j && cJSON_IsNumber(date_j)) ? (long long)date_j->valuedouble : now);
            sqlite3_bind_int64(stmt, 7, now);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
        long long id = sqlite3_last_insert_rowid(app->db);
        cJSON_Delete(root);
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "id", (double)id);
        cJSON_AddStringToObject(resp, "status", "created");
        send_json(c, 201, resp);
    } else {
        char contact_buf[16] = {0}, deal_buf[16] = {0};
        mg_http_get_var(&hm->query, "contact_id", contact_buf, sizeof(contact_buf));
        mg_http_get_var(&hm->query, "deal_id",    deal_buf,    sizeof(deal_buf));
        int cf = atoi(contact_buf);
        int df = atoi(deal_buf);

        char sql[512];
        if (cf > 0)
            snprintf(sql, sizeof(sql),
                "SELECT a.id,a.type,a.contact_id,a.deal_id,a.subject,a.description,a.date,a.completed,a.created_at,"
                "COALESCE(c.display_name,'') FROM activities a LEFT JOIN contacts c ON a.contact_id=c.id "
                "WHERE a.contact_id=%d ORDER BY a.date DESC LIMIT 100;", cf);
        else if (df > 0)
            snprintf(sql, sizeof(sql),
                "SELECT a.id,a.type,a.contact_id,a.deal_id,a.subject,a.description,a.date,a.completed,a.created_at,"
                "COALESCE(c.display_name,'') FROM activities a LEFT JOIN contacts c ON a.contact_id=c.id "
                "WHERE a.deal_id=%d ORDER BY a.date DESC LIMIT 100;", df);
        else
            snprintf(sql, sizeof(sql),
                "SELECT a.id,a.type,a.contact_id,a.deal_id,a.subject,a.description,a.date,a.completed,a.created_at,"
                "COALESCE(c.display_name,'') FROM activities a LEFT JOIN contacts c ON a.contact_id=c.id "
                "ORDER BY a.date DESC LIMIT 100;");

        sqlite3_stmt *stmt;
        cJSON *arr = cJSON_CreateArray();
        if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                cJSON *row = cJSON_CreateObject();
                cJSON_AddNumberToObject(row, "id",           sqlite3_column_int(stmt, 0));
                cJSON_AddStringToObject(row, "type",         (const char*)sqlite3_column_text(stmt, 1) ?: "");
                cJSON_AddNumberToObject(row, "contact_id",   sqlite3_column_int(stmt, 2));
                cJSON_AddNumberToObject(row, "deal_id",      sqlite3_column_int(stmt, 3));
                cJSON_AddStringToObject(row, "subject",      (const char*)sqlite3_column_text(stmt, 4) ?: "");
                cJSON_AddStringToObject(row, "description",  (const char*)sqlite3_column_text(stmt, 5) ?: "");
                cJSON_AddNumberToObject(row, "date",         sqlite3_column_int64(stmt, 6));
                cJSON_AddBoolToObject(row,   "completed",    sqlite3_column_int(stmt, 7));
                cJSON_AddNumberToObject(row, "created_at",   sqlite3_column_int64(stmt, 8));
                cJSON_AddStringToObject(row, "contact_name", (const char*)sqlite3_column_text(stmt, 9) ?: "");
                cJSON_AddItemToArray(arr, row);
            }
            sqlite3_finalize(stmt);
        }
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddItemToObject(resp, "activities", arr);
        send_json(c, 200, resp);
    }
}

void route_activity_update(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id) {
    if (mg_vcasecmp(&hm->method, "DELETE") == 0) {
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(app->db, "DELETE FROM activities WHERE id=?;", -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
        cJSON *ok = cJSON_CreateObject();
        cJSON_AddStringToObject(ok, "status", "deleted");
        send_json(c, 200, ok);
        return;
    }
    cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
    if (!root) { send_error(c, 400, "Invalid JSON"); return; }
    cJSON *completed_j   = cJSON_GetObjectItem(root, "completed");
    cJSON *subject_j     = cJSON_GetObjectItem(root, "subject");
    cJSON *description_j = cJSON_GetObjectItem(root, "description");
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(app->db,
        "UPDATE activities SET completed=?, subject=COALESCE(?,subject), description=COALESCE(?,description) WHERE id=?;",
        -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, (completed_j && cJSON_IsBool(completed_j)) ? cJSON_IsTrue(completed_j) : 0);
        if (subject_j && cJSON_IsString(subject_j))
            sqlite3_bind_text(stmt, 2, subject_j->valuestring, -1, SQLITE_TRANSIENT);
        else
            sqlite3_bind_null(stmt, 2);
        if (description_j && cJSON_IsString(description_j))
            sqlite3_bind_text(stmt, 3, description_j->valuestring, -1, SQLITE_TRANSIENT);
        else
            sqlite3_bind_null(stmt, 3);
        sqlite3_bind_int(stmt, 4, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    cJSON_Delete(root);
    cJSON *ok = cJSON_CreateObject();
    cJSON_AddStringToObject(ok, "status", "updated");
    send_json(c, 200, ok);
}

// ============================================================
// LEADS — CRM Lead Management (pre-deal prospects)
// ============================================================
void route_leads(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    if (mg_vcasecmp(&hm->method, "POST") == 0) {
        cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
        if (!root) { send_error(c, 400, "Invalid JSON"); return; }

        cJSON *name_j    = cJSON_GetObjectItem(root, "name");
        cJSON *company_j = cJSON_GetObjectItem(root, "company");
        cJSON *email_j   = cJSON_GetObjectItem(root, "email");
        cJSON *phone_j   = cJSON_GetObjectItem(root, "phone");
        cJSON *source_j  = cJSON_GetObjectItem(root, "source");
        cJSON *notes_j   = cJSON_GetObjectItem(root, "notes");

        if (!name_j || !cJSON_IsString(name_j)) { cJSON_Delete(root); send_error(c, 400, "name required"); return; }

        const char *sql = "INSERT INTO leads (name, company, email, phone, source, status, notes, created_at) "
                          "VALUES (?,?,?,?,?,?,?,?);";
        sqlite3_stmt *stmt;
        long long now = (long long)time(NULL);
        if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, name_j->valuestring, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, (company_j && cJSON_IsString(company_j)) ? company_j->valuestring : "", -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, (email_j && cJSON_IsString(email_j))     ? email_j->valuestring   : "", -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, (phone_j && cJSON_IsString(phone_j))     ? phone_j->valuestring   : "", -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 5, (source_j && cJSON_IsString(source_j))   ? source_j->valuestring  : "website", -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 6, "new", -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 7, (notes_j && cJSON_IsString(notes_j))     ? notes_j->valuestring   : "", -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(stmt, 8, now);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
        long long new_id = sqlite3_last_insert_rowid(app->db);
        cJSON_Delete(root);
        audit_log_action(app, "LEAD_CREATED", "lead", (int)new_id, "Lead created");
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "id", (double)new_id);
        cJSON_AddStringToObject(resp, "status", "created");
        send_json(c, 201, resp);
    } else {
        int limit = 50, offset = 0;
        get_pagination(hm, &limit, &offset);
        char sql[256];
        snprintf(sql, sizeof(sql),
            "SELECT id,name,company,email,phone,source,status,assigned_to,notes,contact_id,deal_id,created_at "
            "FROM leads WHERE status != 'lost' ORDER BY created_at DESC LIMIT %d OFFSET %d;",
            limit, offset);
        sqlite3_stmt *stmt;
        cJSON *arr = cJSON_CreateArray();
        if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                cJSON *row = cJSON_CreateObject();
                cJSON_AddNumberToObject(row, "id",          sqlite3_column_int(stmt, 0));
                cJSON_AddStringToObject(row, "name",        (const char*)sqlite3_column_text(stmt, 1) ?: "");
                cJSON_AddStringToObject(row, "company",     (const char*)sqlite3_column_text(stmt, 2) ?: "");
                cJSON_AddStringToObject(row, "email",       (const char*)sqlite3_column_text(stmt, 3) ?: "");
                cJSON_AddStringToObject(row, "phone",       (const char*)sqlite3_column_text(stmt, 4) ?: "");
                cJSON_AddStringToObject(row, "source",      (const char*)sqlite3_column_text(stmt, 5) ?: "");
                cJSON_AddStringToObject(row, "status",      (const char*)sqlite3_column_text(stmt, 6) ?: "");
                cJSON_AddStringToObject(row, "assigned_to", (const char*)sqlite3_column_text(stmt, 7) ?: "");
                cJSON_AddStringToObject(row, "notes",       (const char*)sqlite3_column_text(stmt, 8) ?: "");
                cJSON_AddNumberToObject(row, "contact_id",  sqlite3_column_int(stmt, 9));
                cJSON_AddNumberToObject(row, "deal_id",     sqlite3_column_int(stmt, 10));
                cJSON_AddNumberToObject(row, "created_at",  sqlite3_column_int64(stmt, 11));
                cJSON_AddItemToArray(arr, row);
            }
            sqlite3_finalize(stmt);
        }
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddItemToObject(resp, "leads", arr);
        send_json(c, 200, resp);
    }
}

void route_lead_update(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id) {
    if (mg_vcasecmp(&hm->method, "DELETE") == 0) {
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(app->db, "UPDATE leads SET status='lost' WHERE id=?;", -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
        audit_log_action(app, "LEAD_DELETED", "lead", id, "Marked lost");
        cJSON *ok = cJSON_CreateObject();
        cJSON_AddStringToObject(ok, "status", "deleted");
        send_json(c, 200, ok);
        return;
    }
    cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
    if (!root) { send_error(c, 400, "Invalid JSON"); return; }

    // Convert lead to deal
    cJSON *convert_j = cJSON_GetObjectItem(root, "convert_to_deal");
    if (convert_j && cJSON_IsTrue(convert_j)) {
        cJSON *amount_j = cJSON_GetObjectItem(root, "amount");
        char name_buf[256] = {0}, company_buf[256] = {0};
        sqlite3_stmt *sel;
        if (sqlite3_prepare_v2(app->db, "SELECT name, company FROM leads WHERE id=?;", -1, &sel, NULL) == SQLITE_OK) {
            sqlite3_bind_int(sel, 1, id);
            if (sqlite3_step(sel) == SQLITE_ROW) {
                const char *n = (const char*)sqlite3_column_text(sel, 0);
                const char *co = (const char*)sqlite3_column_text(sel, 1);
                if (n)  strncpy(name_buf,    n,  255);
                if (co) strncpy(company_buf, co, 255);
            }
            sqlite3_finalize(sel);
        }
        char title[512];
        if (company_buf[0])
            snprintf(title, sizeof(title), "%s — %s", name_buf, company_buf);
        else
            snprintf(title, sizeof(title), "%s", name_buf);

        long long now = (long long)time(NULL);
        long long deal_id = 0;
        sqlite3_stmt *ds;
        if (sqlite3_prepare_v2(app->db,
            "INSERT INTO deals (title, stage, amount, probability, created_at) VALUES (?,?,?,50,?);",
            -1, &ds, NULL) == SQLITE_OK) {
            sqlite3_bind_text(ds,  1, title, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(ds,  2, "contacted", -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(ds, 3, (amount_j && cJSON_IsNumber(amount_j)) ? (long long)amount_j->valuedouble : 0);
            sqlite3_bind_int64(ds, 4, now);
            sqlite3_step(ds);
            sqlite3_finalize(ds);
            deal_id = sqlite3_last_insert_rowid(app->db);
        }
        sqlite3_stmt *upd;
        if (sqlite3_prepare_v2(app->db, "UPDATE leads SET status='converted', deal_id=? WHERE id=?;", -1, &upd, NULL) == SQLITE_OK) {
            sqlite3_bind_int64(upd, 1, deal_id);
            sqlite3_bind_int(upd, 2, id);
            sqlite3_step(upd);
            sqlite3_finalize(upd);
        }
        cJSON_Delete(root);
        audit_log_action(app, "LEAD_CONVERTED", "lead", id, "Converted to deal");
        cJSON *ok = cJSON_CreateObject();
        cJSON_AddStringToObject(ok, "status", "converted");
        cJSON_AddNumberToObject(ok, "deal_id", (double)deal_id);
        send_json(c, 200, ok);
        return;
    }

    cJSON *status_j = cJSON_GetObjectItem(root, "status");
    cJSON *notes_j  = cJSON_GetObjectItem(root, "notes");
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(app->db,
        "UPDATE leads SET status=COALESCE(?,status), notes=COALESCE(?,notes) WHERE id=?;",
        -1, &stmt, NULL) == SQLITE_OK) {
        if (status_j && cJSON_IsString(status_j)) sqlite3_bind_text(stmt, 1, status_j->valuestring, -1, SQLITE_TRANSIENT);
        else sqlite3_bind_null(stmt, 1);
        if (notes_j && cJSON_IsString(notes_j)) sqlite3_bind_text(stmt, 2, notes_j->valuestring, -1, SQLITE_TRANSIENT);
        else sqlite3_bind_null(stmt, 2);
        sqlite3_bind_int(stmt, 3, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    cJSON_Delete(root);
    audit_log_action(app, "LEAD_UPDATED", "lead", id, "Updated");
    cJSON *ok = cJSON_CreateObject();
    cJSON_AddStringToObject(ok, "status", "updated");
    send_json(c, 200, ok);
}
