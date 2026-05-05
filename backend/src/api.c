#include "../include/qmanage.h"
#include "../include/banking.h"
#include "../include/migration.h"
#include "../include/backup.h"
#include "../include/erp.h"
#include <ctype.h>

// ============================================================
// Mongoose Compatibility Helpers
// ============================================================
int mg_vcasecmp(const struct mg_str *s1, const char *s2) {
    size_t n2 = strlen(s2);
    size_t i;

    if (s1->len != n2) return -1;

    // s1->buf is NOT null-terminated; compare character-by-character
    for (i = 0; i < n2; i++) {
        unsigned char c1 = (unsigned char) s1->buf[i];
        unsigned char c2 = (unsigned char) s2[i];
        int d = tolower(c1) - tolower(c2);
        if (d != 0) return d;
    }

    return 0;
}

// ============================================================
// Pagination helper — parses ?limit=50&offset=0 from query string
// ============================================================
void get_pagination(struct mg_http_message *hm, int *limit, int *offset) {
    char lbuf[16] = "50", obuf[16] = "0";
    mg_http_get_var(&hm->query, "limit",  lbuf, sizeof(lbuf));
    mg_http_get_var(&hm->query, "offset", obuf, sizeof(obuf));
    *limit  = atoi(lbuf);
    *offset = atoi(obuf);
    if (*limit  <= 0 || *limit  > 500) *limit  = 50;
    if (*offset < 0)                   *offset = 0;
}

// ============================================================
// Utilities
// ============================================================
void send_json(struct mg_connection *c, int status, cJSON *data) {
    char *json_str = cJSON_PrintUnformatted(data);
    mg_http_reply(c, status,
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n",
        "%s", json_str);
    free(json_str);
    cJSON_Delete(data);
}

void send_error(struct mg_connection *c, int status, const char *msg) {
    cJSON *err = cJSON_CreateObject();
    cJSON_AddStringToObject(err, "error", msg);
    send_json(c, status, err);
}

// ============================================================
// Dashboard — Live from Double-Entry Ledger (ENHANCED)
// ============================================================
void route_dashboard(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    cJSON *res = cJSON_CreateObject();

    // Total Assets
    double assets = 0;
    cJSON *q = db_query(app,
        "SELECT COALESCE(SUM(debit - credit), 0) as bal "
        "FROM journal_lines l JOIN accounts a ON l.account_id = a.id "
        "WHERE a.type = 1;");
    if (q && cJSON_GetArraySize(q) > 0) {
        cJSON *row = cJSON_GetArrayItem(q, 0);
        cJSON *val = row ? cJSON_GetObjectItem(row, "bal") : NULL;
        if (val) assets = val->valuedouble;
    }
    cJSON_AddNumberToObject(res, "total_assets", assets);
    cJSON_Delete(q);

    // Total Income
    double income = 0;
    q = db_query(app,
        "SELECT COALESCE(SUM(credit - debit), 0) as bal "
        "FROM journal_lines l JOIN accounts a ON l.account_id = a.id "
        "WHERE a.type = 4;");
    if (q && cJSON_GetArraySize(q) > 0) {
        cJSON *row = cJSON_GetArrayItem(q, 0);
        cJSON *val = row ? cJSON_GetObjectItem(row, "bal") : NULL;
        if (val) income = val->valuedouble;
    }
    cJSON_AddNumberToObject(res, "total_income", income);
    cJSON_Delete(q);

    // Total Expenses
    double expenses = 0;
    q = db_query(app,
        "SELECT COALESCE(SUM(debit - credit), 0) as bal "
        "FROM journal_lines l JOIN accounts a ON l.account_id = a.id "
        "WHERE a.type = 5;");
    if (q && cJSON_GetArraySize(q) > 0) {
        cJSON *row = cJSON_GetArrayItem(q, 0);
        cJSON *val = row ? cJSON_GetObjectItem(row, "bal") : NULL;
        if (val) expenses = val->valuedouble;
    }
    cJSON_AddNumberToObject(res, "total_expenses", expenses);
    cJSON_Delete(q);

    // Liabilities
    double liabilities = 0;
    q = db_query(app,
        "SELECT COALESCE(SUM(credit - debit), 0) as bal "
        "FROM journal_lines l JOIN accounts a ON l.account_id = a.id "
        "WHERE a.type = 2;");
    if (q && cJSON_GetArraySize(q) > 0) {
        cJSON *row = cJSON_GetArrayItem(q, 0);
        cJSON *val = row ? cJSON_GetObjectItem(row, "bal") : NULL;
        if (val) liabilities = val->valuedouble;
    }
    cJSON_AddNumberToObject(res, "total_liabilities", liabilities);
    cJSON_Delete(q);

    // Net Profit
    cJSON_AddNumberToObject(res, "net_profit", income - expenses);

    // Counts
    cJSON *counts = db_query(app,
        "SELECT "
        "(SELECT COUNT(*) FROM contacts) as total_contacts, "
        "(SELECT COUNT(*) FROM invoices) as total_invoices, "
        "(SELECT COUNT(*) FROM invoices WHERE status IN ('sent','partial','overdue')) as unpaid_invoices, "
        "(SELECT COUNT(*) FROM bills) as total_bills, "
        "(SELECT COUNT(*) FROM bills WHERE status IN ('received','partial','overdue')) as unpaid_bills, "
        "(SELECT COUNT(*) FROM deals WHERE stage NOT IN ('won','lost')) as active_deals, "
        "(SELECT COUNT(*) FROM quotes WHERE status IN ('draft','sent')) as open_quotes, "
        "(SELECT COALESCE(SUM(balance_due), 0) FROM invoices WHERE status NOT IN ('paid','void')) as total_receivable, "
        "(SELECT COALESCE(SUM(balance_due), 0) FROM bills WHERE status NOT IN ('paid','void')) as total_payable, "
        "(SELECT COUNT(*) FROM bank_transactions WHERE is_matched = 0) as unmatched_transactions, "
        "(SELECT COUNT(*) FROM items WHERE stock <= reorder_level AND stock >= 0) as low_stock_items;");
    if (counts && cJSON_GetArraySize(counts) > 0) {
        cJSON *c_row = cJSON_DetachItemFromArray(counts, 0);
        cJSON_AddItemToObject(res, "counts", c_row);
    }
    cJSON_Delete(counts);

    // Recent invoices
    cJSON *recent_inv = db_query(app,
        "SELECT i.invoice_num, c.display_name as customer, i.total, i.status, i.date "
        "FROM invoices i LEFT JOIN contacts c ON i.customer_id = c.id "
        "ORDER BY i.date DESC LIMIT 5;");
    cJSON_AddItemToObject(res, "recent_invoices", recent_inv);

    // CRM Pipeline summary
    cJSON *pipeline = db_query(app,
        "SELECT stage, COUNT(*) as count, COALESCE(SUM(amount), 0) as total "
        "FROM deals GROUP BY stage;");
    cJSON_AddItemToObject(res, "pipeline", pipeline);

    send_json(c, 200, res);
}

// ============================================================
// Contacts — Customers + Vendors + Leads (PARAMETERIZED)
// ============================================================
void route_contacts(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    if (mg_vcasecmp(&hm->method, "POST") == 0) {
        cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
        if (!root) { send_error(c, 400, "Invalid JSON"); return; }

        cJSON *name = cJSON_GetObjectItem(root, "display_name");
        if (!name || !cJSON_IsString(name)) {
            send_error(c, 400, "display_name is required");
            cJSON_Delete(root); return;
        }

        cJSON *type = cJSON_GetObjectItem(root, "type");
        cJSON *email = cJSON_GetObjectItem(root, "email");
        cJSON *phone = cJSON_GetObjectItem(root, "phone");
        cJSON *gstin = cJSON_GetObjectItem(root, "gstin");
        cJSON *company = cJSON_GetObjectItem(root, "company_name");
        cJSON *pan = cJSON_GetObjectItem(root, "pan");
        cJSON *b_addr = cJSON_GetObjectItem(root, "billing_address");
        cJSON *b_city = cJSON_GetObjectItem(root, "billing_city");
        cJSON *b_state = cJSON_GetObjectItem(root, "billing_state");

        sqlite3_stmt *stmt;
        const char *sql = "INSERT INTO contacts (display_name, type, email, phone, gstin, company_name, pan, "
                          "billing_address, billing_city, billing_state, created_at) "
                          "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, strftime('%s','now'));";
        if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, name->valuestring, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, (type && cJSON_IsString(type)) ? type->valuestring : "customer", -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, (email && cJSON_IsString(email)) ? email->valuestring : "", -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, (phone && cJSON_IsString(phone)) ? phone->valuestring : "", -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 5, (gstin && cJSON_IsString(gstin)) ? gstin->valuestring : "", -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 6, (company && cJSON_IsString(company)) ? company->valuestring : "", -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 7, (pan && cJSON_IsString(pan)) ? pan->valuestring : "", -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 8, (b_addr && cJSON_IsString(b_addr)) ? b_addr->valuestring : "", -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 9, (b_city && cJSON_IsString(b_city)) ? b_city->valuestring : "", -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 10, (b_state && cJSON_IsString(b_state)) ? b_state->valuestring : "", -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
        cJSON_Delete(root);

        cJSON *ok = cJSON_CreateObject();
        cJSON_AddStringToObject(ok, "status", "created");
        cJSON_AddNumberToObject(ok, "id", sqlite3_last_insert_rowid(app->db));
        send_json(c, 201, ok);
    } else {
        int limit, offset;
        get_pagination(hm, &limit, &offset);
        char sql[256];
        snprintf(sql, sizeof(sql),
            "SELECT * FROM contacts WHERE is_active = 1 ORDER BY display_name LIMIT %d OFFSET %d;",
            limit, offset);
        send_json(c, 200, db_query(app, sql));
    }
}

// ============================================================
// Items (PARAMETERIZED)
// ============================================================
void route_items(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    if (mg_vcasecmp(&hm->method, "POST") == 0) {
        cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
        if (!root) { send_error(c, 400, "Invalid JSON"); return; }

        cJSON *name = cJSON_GetObjectItem(root, "name");
        if (!name || !cJSON_IsString(name)) {
            send_error(c, 400, "Item name is required");
            cJSON_Delete(root); return;
        }

        cJSON *sku = cJSON_GetObjectItem(root, "sku");
        cJSON *price = cJSON_GetObjectItem(root, "price");
        cJSON *cost = cJSON_GetObjectItem(root, "cost_price");
        cJSON *tax = cJSON_GetObjectItem(root, "tax_rate");
        cJSON *hsn = cJSON_GetObjectItem(root, "hsn_code");
        cJSON *unit = cJSON_GetObjectItem(root, "unit");
        cJSON *stock = cJSON_GetObjectItem(root, "stock");

        sqlite3_stmt *stmt;
        const char *sql = "INSERT INTO items (name, sku, price, cost_price, tax_rate, hsn_code, unit, stock, created_at) "
                          "VALUES (?, ?, ?, ?, ?, ?, ?, ?, strftime('%s','now'));";
        if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, name->valuestring, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, (sku && cJSON_IsString(sku)) ? sku->valuestring : "", -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(stmt, 3, (price && cJSON_IsNumber(price)) ? (int64_t)(price->valuedouble) : 0);
            sqlite3_bind_int64(stmt, 4, (cost && cJSON_IsNumber(cost)) ? (int64_t)(cost->valuedouble) : 0);
            sqlite3_bind_double(stmt, 5, (tax && cJSON_IsNumber(tax)) ? tax->valuedouble : 18.0);
            sqlite3_bind_text(stmt, 6, (hsn && cJSON_IsString(hsn)) ? hsn->valuestring : "", -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 7, (unit && cJSON_IsString(unit)) ? unit->valuestring : "pcs", -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 8, (stock && cJSON_IsNumber(stock)) ? stock->valueint : 0);
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
        char sql[256];
        snprintf(sql, sizeof(sql),
            "SELECT * FROM items WHERE is_active = 1 ORDER BY name LIMIT %d OFFSET %d;",
            limit, offset);
        send_json(c, 200, db_query(app, sql));
    }
}

// ============================================================
// Invoices (PARAMETERIZED + Auto Ledger)
// ============================================================
void route_invoices(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    if (mg_vcasecmp(&hm->method, "POST") == 0) {
        cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
        if (!root) { send_error(c, 400, "Invalid JSON"); return; }

        cJSON *num = cJSON_GetObjectItem(root, "invoice_num");
        cJSON *cust = cJSON_GetObjectItem(root, "customer_id");
        cJSON *total_j = cJSON_GetObjectItem(root, "total");
        cJSON *date_j = cJSON_GetObjectItem(root, "date");
        cJSON *notes_j = cJSON_GetObjectItem(root, "notes");
        cJSON *subtotal_j = cJSON_GetObjectItem(root, "subtotal");
        cJSON *tax_j = cJSON_GetObjectItem(root, "tax");

        if (!num || !cust || !total_j) {
            send_error(c, 400, "Missing invoice_num, customer_id, or total");
            cJSON_Delete(root); return;
        }

        int64_t total = (int64_t)(total_j->valuedouble);
        int64_t date = date_j ? (int64_t)date_j->valuedouble : (int64_t)time(NULL);
        int64_t subtotal = subtotal_j ? (int64_t)(subtotal_j->valuedouble) : total;
        int64_t tax = tax_j ? (int64_t)(tax_j->valuedouble) : 0;

        sqlite3_stmt *stmt;
        const char *sql = "INSERT INTO invoices (invoice_num, customer_id, subtotal, tax, total, balance_due, date, status, notes, created_at) "
                          "VALUES (?, ?, ?, ?, ?, ?, ?, 'sent', ?, strftime('%s','now'));";
        if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, num->valuestring, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 2, cust->valueint);
            sqlite3_bind_int64(stmt, 3, subtotal);
            sqlite3_bind_int64(stmt, 4, tax);
            sqlite3_bind_int64(stmt, 5, total);
            sqlite3_bind_int64(stmt, 6, total);
            sqlite3_bind_int64(stmt, 7, date);
            sqlite3_bind_text(stmt, 8, (notes_j && cJSON_IsString(notes_j)) ? notes_j->valuestring : "", -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }

        int64_t invoice_id = sqlite3_last_insert_rowid(app->db);

        // Save invoice line items (if provided)
        cJSON *items_arr = cJSON_GetObjectItem(root, "items");
        if (items_arr && cJSON_IsArray(items_arr)) {
            sqlite3_stmt *line_stmt;
            const char *line_sql = "INSERT INTO invoice_lines (invoice_id, item_id, description, quantity, rate, tax_rate, amount) "
                                   "VALUES (?, ?, ?, ?, ?, ?, ?);";
            if (sqlite3_prepare_v2(app->db, line_sql, -1, &line_stmt, NULL) == SQLITE_OK) {
                cJSON *item;
                cJSON_ArrayForEach(item, items_arr) {
                    cJSON *desc = cJSON_GetObjectItem(item, "description");
                    cJSON *qty = cJSON_GetObjectItem(item, "quantity");
                    cJSON *rate = cJSON_GetObjectItem(item, "rate");
                    cJSON *tax_r = cJSON_GetObjectItem(item, "tax_rate");
                    cJSON *amt = cJSON_GetObjectItem(item, "amount");
                    cJSON *item_id = cJSON_GetObjectItem(item, "item_id");

                    sqlite3_reset(line_stmt);
                    sqlite3_bind_int64(line_stmt, 1, invoice_id);
                    sqlite3_bind_int(line_stmt, 2, (item_id && cJSON_IsNumber(item_id)) ? item_id->valueint : 0);
                    sqlite3_bind_text(line_stmt, 3, (desc && cJSON_IsString(desc)) ? desc->valuestring : "", -1, SQLITE_TRANSIENT);
                    sqlite3_bind_double(line_stmt, 4, (qty && cJSON_IsNumber(qty)) ? qty->valuedouble : 1);
                    sqlite3_bind_int64(line_stmt, 5, (rate && cJSON_IsNumber(rate)) ? (int64_t)rate->valuedouble : 0);
                    sqlite3_bind_double(line_stmt, 6, (tax_r && cJSON_IsNumber(tax_r)) ? tax_r->valuedouble : 0);
                    sqlite3_bind_int64(line_stmt, 7, (amt && cJSON_IsNumber(amt)) ? (int64_t)amt->valuedouble : 0);
                    sqlite3_step(line_stmt);
                }
                sqlite3_finalize(line_stmt);
            }
        }

        // Auto-create ledger entry: Debit AR(4), Credit Sales(3)
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
        snprintf(memo, sizeof(memo), "Invoice %s", num->valuestring);
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
            "SELECT i.*, c.display_name as customer_name "
            "FROM invoices i LEFT JOIN contacts c ON i.customer_id = c.id "
            "ORDER BY i.date DESC LIMIT %d OFFSET %d;", limit, offset);
        send_json(c, 200, db_query(app, sql));
    }
}

// ============================================================
// Expenses (PARAMETERIZED + Auto Ledger)
// ============================================================
static void route_expenses(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    if (mg_vcasecmp(&hm->method, "POST") == 0) {
        cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
        if (!root) { send_error(c, 400, "Invalid JSON"); return; }

        cJSON *cat = cJSON_GetObjectItem(root, "category");
        cJSON *amt_j = cJSON_GetObjectItem(root, "amount");
        cJSON *date_j = cJSON_GetObjectItem(root, "date");

        if (!amt_j) { send_error(c, 400, "Amount is required"); cJSON_Delete(root); return; }

        int64_t amount = (int64_t)(amt_j->valuedouble);
        int64_t date = date_j ? (int64_t)date_j->valuedouble : (int64_t)time(NULL);

        int account_id = 8;
        if (cat && cJSON_IsString(cat)) {
            if (strcmp(cat->valuestring, "Rent") == 0) account_id = 9;
            else if (strcmp(cat->valuestring, "Utilities") == 0) account_id = 10;
            else if (strcmp(cat->valuestring, "Advertising") == 0) account_id = 19;
            else if (strcmp(cat->valuestring, "Travel") == 0) account_id = 20;
            else if (strcmp(cat->valuestring, "Internet") == 0) account_id = 21;
        }

        cJSON *lines = cJSON_CreateArray();
        cJSON *dr = cJSON_CreateObject();
        cJSON_AddNumberToObject(dr, "account_id", account_id);
        cJSON_AddNumberToObject(dr, "debit", amount);
        cJSON_AddNumberToObject(dr, "credit", 0);
        cJSON_AddItemToArray(lines, dr);

        cJSON *cr = cJSON_CreateObject();
        cJSON_AddNumberToObject(cr, "account_id", 1);
        cJSON_AddNumberToObject(cr, "debit", 0);
        cJSON_AddNumberToObject(cr, "credit", amount);
        cJSON_AddItemToArray(lines, cr);

        char memo[128];
        snprintf(memo, sizeof(memo), "Expense: %s",
                 (cat && cJSON_IsString(cat)) ? cat->valuestring : "General");
        ledger_record_entry(app, memo, date, lines);
        cJSON_Delete(lines);
        cJSON_Delete(root);

        cJSON *ok = cJSON_CreateObject();
        cJSON_AddStringToObject(ok, "status", "recorded");
        send_json(c, 201, ok);
    } else {
        cJSON *list = db_query(app,
            "SELECT e.id, e.entry_date, e.memo, "
            "l.debit as amount, a.name as category "
            "FROM journal_entries e "
            "JOIN journal_lines l ON l.journal_id = e.id "
            "JOIN accounts a ON l.account_id = a.id "
            "WHERE a.type = 5 AND l.debit > 0 "
            "ORDER BY e.entry_date DESC;");
        send_json(c, 200, list);
    }
}

// ============================================================
// Ledger + Accounts
// ============================================================
void route_ledger(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    if (mg_vcasecmp(&hm->method, "POST") == 0) {
        // Manual Journal Entry from frontend form
        cJSON *root = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
        if (!root) { send_error(c, 400, "Invalid JSON"); return; }

        cJSON *memo_j = cJSON_GetObjectItem(root, "memo");
        cJSON *date_j = cJSON_GetObjectItem(root, "date");
        cJSON *lines_j = cJSON_GetObjectItem(root, "lines");

        if (!memo_j || !cJSON_IsString(memo_j) || !lines_j || !cJSON_IsArray(lines_j)) {
            send_error(c, 400, "Required: memo (string), lines (array)");
            cJSON_Delete(root); return;
        }

        int64_t date = date_j ? (int64_t)date_j->valuedouble : (int64_t)time(NULL);
        FinResult result = ledger_record_entry(app, memo_j->valuestring, date, lines_j);
        cJSON_Delete(root);

        if (result == FIN_OK) {
            cJSON *ok = cJSON_CreateObject();
            cJSON_AddStringToObject(ok, "status", "posted");
            cJSON_AddStringToObject(ok, "message", "Journal entry posted to ledger");
            send_json(c, 201, ok);
        } else if (result == FIN_ERR_BALANCE) {
            send_error(c, 400, "Entry rejected: Debits do not equal Credits");
        } else {
            send_error(c, 500, "Database error recording journal entry");
        }
    } else {
        // GET — list journal lines
        cJSON *list = db_query(app,
            "SELECT e.id, e.entry_date, e.memo, e.source_module, "
            "l.debit, l.credit, a.name as account_name, a.code as account_code "
            "FROM journal_entries e "
            "JOIN journal_lines l ON l.journal_id = e.id "
            "JOIN accounts a ON l.account_id = a.id "
            "ORDER BY e.entry_date DESC, e.id DESC LIMIT 200;");
        send_json(c, 200, list);
    }
}

void route_accounts(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    cJSON *list = db_query(app,
        "SELECT a.*, "
        "COALESCE(SUM(l.debit), 0) as total_debit, "
        "COALESCE(SUM(l.credit), 0) as total_credit, "
        "COALESCE(SUM(l.debit - l.credit), 0) as balance "
        "FROM accounts a LEFT JOIN journal_lines l ON a.id = l.account_id "
        "GROUP BY a.id ORDER BY a.code;");
    send_json(c, 200, list);
}

// ============================================================
// Backup Routes
// ============================================================
static void route_backup(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    if (mg_vcasecmp(&hm->method, "POST") == 0) {
        route_backup_create(c, hm, app);
    } else {
        route_backup_list(c, hm, app);
    }
}

// ============================================================
// Main API Router — 30+ Endpoints
// ============================================================
void handle_api_request(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    if (mg_vcasecmp(&hm->method, "OPTIONS") == 0) {
        mg_http_reply(c, 200,
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type, X-Import-Type, X-Auth-Token\r\n",
            "");
        return;
    }

    // ---- Auth endpoints are always public ----
    if (mg_vcasecmp(&hm->uri, "/api/auth/login") == 0) {
        route_auth_login(c, hm, app);
        return;
    }

    // ---- All other endpoints require a valid session token ----
    int user_id = auth_validate(c, hm, app);
    if (user_id == 0) return; // auth_validate already sent 401

    if (mg_match(hm->uri, mg_str("/api/auth/logout"), NULL)) {
        route_auth_logout(c, hm, app);
        return;
    }
    if (mg_match(hm->uri, mg_str("/api/auth/change-password"), NULL)) {
        route_auth_change_password(c, hm, app, user_id);
        return;
    }

    // ---- Core Modules ----
    if      (mg_match(hm->uri, mg_str("/api/dashboard"), NULL))          route_dashboard(c, hm, app);
    else if (mg_match(hm->uri, mg_str("/api/contacts"), NULL))           route_contacts(c, hm, app);
    else if (mg_match(hm->uri, mg_str("/api/items"), NULL))              route_items(c, hm, app);
    else if (mg_match(hm->uri, mg_str("/api/invoices"), NULL))           route_invoices(c, hm, app);
    else if (mg_match(hm->uri, mg_str("/api/expenses"), NULL))           route_expenses(c, hm, app);
    else if (mg_match(hm->uri, mg_str("/api/ledger"), NULL))             route_ledger(c, hm, app);
    else if (mg_match(hm->uri, mg_str("/api/accounts"), NULL))           route_accounts(c, hm, app);

    // ---- ERP Modules ----
    else if (mg_match(hm->uri, mg_str("/api/bills"), NULL))              route_bills(c, hm, app);
    else if (mg_match(hm->uri, mg_str("/api/payments/received"), NULL))  route_payments_received(c, hm, app);
    else if (mg_match(hm->uri, mg_str("/api/payments/made"), NULL))      route_payments_made(c, hm, app);
    else if (mg_match(hm->uri, mg_str("/api/quotes"), NULL))             route_quotes(c, hm, app);
    else if (mg_match(hm->uri, mg_str("/api/quotes/convert"), NULL))     route_quote_convert(c, hm, app);
    else if (mg_match(hm->uri, mg_str("/api/deals"), NULL))              route_deals(c, hm, app);
    else if (mg_match(hm->uri, mg_str("/api/inventory"), NULL))          route_inventory(c, hm, app);
    else if (mg_match(hm->uri, mg_str("/api/search"), NULL))             route_search(c, hm, app);

    // ---- Reports Engine ----
    else if (mg_match(hm->uri, mg_str("/api/reports/balance-sheet"), NULL))       route_report_balance_sheet(c, hm, app);
    else if (mg_match(hm->uri, mg_str("/api/reports/trial-balance"), NULL))       route_report_trial_balance(c, hm, app);
    else if (mg_match(hm->uri, mg_str("/api/reports/cash-flow"), NULL))           route_report_cash_flow(c, hm, app);
    else if (mg_match(hm->uri, mg_str("/api/reports/aging"), NULL))               route_report_aging(c, hm, app);
    else if (mg_match(hm->uri, mg_str("/api/reports/sales-by-customer"), NULL))   route_report_sales_by_customer(c, hm, app);
    else if (mg_match(hm->uri, mg_str("/api/reports/expense-by-category"), NULL)) route_report_expense_by_category(c, hm, app);
    else if (mg_match(hm->uri, mg_str("/api/reports/gst-summary"), NULL))         route_report_gst_summary(c, hm, app);

    // ---- Banking (SMS + Gmail Bot) ----
    else if (mg_match(hm->uri, mg_str("/api/banking/sms"), NULL))        route_banking_sms(c, hm, app);
    else if (mg_match(hm->uri, mg_str("/api/banking/reconcile"), NULL))  route_banking_reconcile(c, hm, app);
    else if (mg_match(hm->uri, mg_str("/api/gmail/config"), NULL))       route_gmail_config(c, hm, app);
    else if (mg_match(hm->uri, mg_str("/api/gmail/fetch"), NULL))        route_gmail_fetch(c, hm, app);
    else if (mg_match(hm->uri, mg_str("/api/gmail/status"), NULL))       route_gmail_status(c, hm, app);
    else if (mg_match(hm->uri, mg_str("/api/email/send"), NULL))         route_email_send(c, hm, app);

    // ---- Backup & Sync ----
    else if (mg_match(hm->uri, mg_str("/api/backup"), NULL))             route_backup(c, hm, app);
    else if (mg_match(hm->uri, mg_str("/api/gdrive/config"), NULL))      route_gdrive_config(c, hm, app);
    else if (mg_match(hm->uri, mg_str("/api/gdrive/upload"), NULL))      route_gdrive_upload(c, hm, app);
    else if (mg_match(hm->uri, mg_str("/api/gdrive/list"), NULL))        route_gdrive_list(c, hm, app);

    // ---- Migration ----
    else if (mg_match(hm->uri, mg_str("/api/migration/upload"), NULL))   route_migration_import(c, hm, app);

    // ---- Credit Notes, Vendor Credits, Purchase Orders (P0 FIX: were dead code) ----
    else if (mg_match(hm->uri, mg_str("/api/credit-notes"), NULL))       route_credit_notes(c, hm, app);
    else if (mg_match(hm->uri, mg_str("/api/vendor-credits"), NULL))     route_vendor_credits(c, hm, app);
    else if (mg_match(hm->uri, mg_str("/api/purchase-orders"), NULL))    route_purchase_orders(c, hm, app);

    // ---- Contact CRUD: PUT/DELETE /api/contacts/:id ----
    else if (mg_match(hm->uri, mg_str("/api/contacts/*"), NULL)) {
        struct mg_str id_str = hm->uri;
        id_str.buf += 14; id_str.len -= 14; // skip "/api/contacts/"
        int id = atoi(id_str.buf);
        if (id > 0) route_contact_update(c, hm, app, id);
        else send_error(c, 400, "Invalid contact ID");
    }

    // ---- Invoice CRUD ----
    else if (mg_match(hm->uri, mg_str("/api/invoices/*/void"), NULL)) {
        struct mg_str id_str = hm->uri;
        id_str.buf += 14;
        int id = atoi(id_str.buf);
        if (id > 0) route_invoice_void(c, hm, app, id);
        else send_error(c, 400, "Invalid invoice ID");
    }
    else if (mg_match(hm->uri, mg_str("/api/invoices/*"), NULL)) {
        struct mg_str id_str = hm->uri;
        id_str.buf += 14; id_str.len -= 14; // skip "/api/invoices/"
        int id = atoi(id_str.buf);
        if (id > 0) route_invoice_update(c, hm, app, id);
        else send_error(c, 400, "Invalid invoice ID");
    }

    // ---- Bill CRUD ----
    else if (mg_match(hm->uri, mg_str("/api/bills/*"), NULL)) {
        struct mg_str id_str = hm->uri;
        id_str.buf += 11; id_str.len -= 11; // skip "/api/bills/"
        int id = atoi(id_str.buf);
        if (id > 0) {
            if (mg_vcasecmp(&hm->method, "DELETE") == 0) route_bill_delete(c, hm, app, id);
            else route_bill_update(c, hm, app, id);
        } else send_error(c, 400, "Invalid bill ID");
    }

    // ---- Item CRUD ----
    else if (mg_match(hm->uri, mg_str("/api/items/*"), NULL)) {
        struct mg_str id_str = hm->uri;
        id_str.buf += 11; id_str.len -= 11; // skip "/api/items/"
        int id = atoi(id_str.buf);
        if (id > 0) {
            if (mg_vcasecmp(&hm->method, "DELETE") == 0) route_item_delete(c, hm, app, id);
            else route_item_update(c, hm, app, id);
        } else send_error(c, 400, "Invalid item ID");
    }

    // ---- Quote CRUD ----
    else if (mg_match(hm->uri, mg_str("/api/quotes/*"), NULL)) {
        struct mg_str id_str = hm->uri;
        id_str.buf += 12; id_str.len -= 12; // skip "/api/quotes/"
        int id = atoi(id_str.buf);
        if (id > 0) {
            if (mg_vcasecmp(&hm->method, "DELETE") == 0) route_quote_delete(c, hm, app, id);
            else route_quote_update(c, hm, app, id);
        } else send_error(c, 400, "Invalid quote ID");
    }

    // ---- Deals CRUD ----
    else if (mg_match(hm->uri, mg_str("/api/deals/*"), NULL)) {
        struct mg_str id_str = hm->uri;
        id_str.buf += 11; id_str.len -= 11; // skip "/api/deals/"
        int id = atoi(id_str.buf);
        if (id > 0) {
            if (mg_vcasecmp(&hm->method, "DELETE") == 0) route_deal_delete(c, hm, app, id);
            else route_deal_update(c, hm, app, id);
        } else send_error(c, 400, "Invalid deal ID");
    }

    // ---- Credit Note CRUD ----
    else if (mg_match(hm->uri, mg_str("/api/credit-notes/*"), NULL)) {
        struct mg_str id_str = hm->uri;
        id_str.buf += 19; id_str.len -= 19; // skip "/api/credit-notes/"
        int id = atoi(id_str.buf);
        if (id > 0) {
            if (mg_vcasecmp(&hm->method, "DELETE") == 0) route_credit_note_delete(c, hm, app, id);
            else route_credit_note_update(c, hm, app, id);
        } else send_error(c, 400, "Invalid credit note ID");
    }

    // ---- Vendor Credit CRUD ----
    else if (mg_match(hm->uri, mg_str("/api/vendor-credits/*"), NULL)) {
        struct mg_str id_str = hm->uri;
        id_str.buf += 21; id_str.len -= 21; // skip "/api/vendor-credits/"
        int id = atoi(id_str.buf);
        if (id > 0) {
            if (mg_vcasecmp(&hm->method, "DELETE") == 0) route_vendor_credit_delete(c, hm, app, id);
            else route_vendor_credit_update(c, hm, app, id);
        } else send_error(c, 400, "Invalid vendor credit ID");
    }

    // ---- Purchase Order CRUD ----
    else if (mg_match(hm->uri, mg_str("/api/purchase-orders/*"), NULL)) {
        struct mg_str id_str = hm->uri;
        id_str.buf += 22; id_str.len -= 22; // skip "/api/purchase-orders/"
        int id = atoi(id_str.buf);
        if (id > 0) {
            if (mg_vcasecmp(&hm->method, "DELETE") == 0) route_purchase_order_delete(c, hm, app, id);
            else route_purchase_order_update(c, hm, app, id);
        } else send_error(c, 400, "Invalid purchase order ID");
    }

    // ---- Audit Trail ----
    else if (mg_match(hm->uri, mg_str("/api/audit"), NULL)) {
        if (mg_vcasecmp(&hm->method, "POST") == 0) {
            int broken = audit_verify_chain(app);
            cJSON *res = cJSON_CreateObject();
            cJSON_AddNumberToObject(res, "broken_links", broken);
            cJSON_AddStringToObject(res, "status", broken == 0 ? "INTACT" : "TAMPERED");
            send_json(c, 200, res);
        } else {
            cJSON *list = db_query(app,
                "SELECT id, action, entity_type, entity_id, details, "
                "substr(hash,1,16) as hash_prefix, substr(prev_hash,1,16) as prev_prefix, "
                "created_at FROM audit_trail ORDER BY id DESC LIMIT 100;");
            send_json(c, 200, list);
        }
    }

    // ---- Activities (CRM Follow-ups) ----
    else if (mg_match(hm->uri, mg_str("/api/activities"), NULL))  route_activities(c, hm, app);

    // ---- Leads ----
    else if (mg_match(hm->uri, mg_str("/api/leads"), NULL))       route_leads(c, hm, app);

    // ---- GST Compliance ----
    else if (mg_match(hm->uri, mg_str("/api/gst/summary"), NULL)) {
        route_gst_summary(c, hm, app);
    }
    else if (mg_match(hm->uri, mg_str("/api/gst/einvoice/*"), NULL)) {
        struct mg_str id_str = hm->uri;
        id_str.buf += 20; id_str.len -= 20; // skip "/api/gst/einvoice/"
        int id = atoi(id_str.buf);
        if (id > 0) route_gst_einvoice(c, hm, app, id);
        else send_error(c, 400, "Invalid invoice ID");
    }
    else if (mg_match(hm->uri, mg_str("/api/gst/eway/*"), NULL)) {
        struct mg_str id_str = hm->uri;
        id_str.buf += 15; id_str.len -= 15; // skip "/api/gst/eway/"
        int id = atoi(id_str.buf);
        if (id > 0) route_gst_eway(c, hm, app, id);
        else send_error(c, 400, "Invalid invoice ID");
    }

    // ---- Activity CRUD: PUT/DELETE /api/activities/:id ----
    else if (mg_match(hm->uri, mg_str("/api/activities/*"), NULL)) {
        struct mg_str id_str = hm->uri;
        id_str.buf += 16; id_str.len -= 16; // skip "/api/activities/"
        int id = atoi(id_str.buf);
        if (id > 0) route_activity_update(c, hm, app, id);
        else send_error(c, 400, "Invalid activity ID");
    }

    // ---- Lead CRUD: PUT/DELETE /api/leads/:id ----
    else if (mg_match(hm->uri, mg_str("/api/leads/*"), NULL)) {
        struct mg_str id_str = hm->uri;
        id_str.buf += 11; id_str.len -= 11; // skip "/api/leads/"
        int id = atoi(id_str.buf);
        if (id > 0) route_lead_update(c, hm, app, id);
        else send_error(c, 400, "Invalid lead ID");
    }

    else send_error(c, 404, "Endpoint not found");
}
