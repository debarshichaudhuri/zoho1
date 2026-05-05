#include "../include/erp.h"

// ============================================================
// BALANCE SHEET — Assets = Liabilities + Equity
// ============================================================
void route_report_balance_sheet(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    cJSON *res = cJSON_CreateObject();

    // Assets (type=1): Debit - Credit
    cJSON *assets = db_query(app,
        "SELECT a.name, a.code, COALESCE(SUM(l.debit - l.credit), 0) as balance "
        "FROM accounts a LEFT JOIN journal_lines l ON a.id = l.account_id "
        "WHERE a.type = 1 GROUP BY a.id HAVING balance != 0 ORDER BY a.code;");
    cJSON_AddItemToObject(res, "assets", assets);

    // Liabilities (type=2): Credit - Debit
    cJSON *liabilities = db_query(app,
        "SELECT a.name, a.code, COALESCE(SUM(l.credit - l.debit), 0) as balance "
        "FROM accounts a LEFT JOIN journal_lines l ON a.id = l.account_id "
        "WHERE a.type = 2 GROUP BY a.id HAVING balance != 0 ORDER BY a.code;");
    cJSON_AddItemToObject(res, "liabilities", liabilities);

    // Equity (type=3): Credit - Debit
    cJSON *equity = db_query(app,
        "SELECT a.name, a.code, COALESCE(SUM(l.credit - l.debit), 0) as balance "
        "FROM accounts a LEFT JOIN journal_lines l ON a.id = l.account_id "
        "WHERE a.type = 3 GROUP BY a.id HAVING balance != 0 ORDER BY a.code;");
    cJSON_AddItemToObject(res, "equity", equity);

    // Totals
    cJSON *totals = db_query(app,
        "SELECT "
        "COALESCE((SELECT SUM(l.debit - l.credit) FROM journal_lines l JOIN accounts a ON l.account_id = a.id WHERE a.type = 1), 0) as total_assets, "
        "COALESCE((SELECT SUM(l.credit - l.debit) FROM journal_lines l JOIN accounts a ON l.account_id = a.id WHERE a.type = 2), 0) as total_liabilities, "
        "COALESCE((SELECT SUM(l.credit - l.debit) FROM journal_lines l JOIN accounts a ON l.account_id = a.id WHERE a.type = 3), 0) as total_equity;");
    if (totals && cJSON_GetArraySize(totals) > 0) {
        cJSON *t = cJSON_DetachItemFromArray(totals, 0);
        cJSON_AddItemToObject(res, "totals", t);
    }
    cJSON_Delete(totals);

    send_json(c, 200, res);
}

// ============================================================
// TRIAL BALANCE — All accounts with their balances
// ============================================================
void route_report_trial_balance(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    cJSON *res = cJSON_CreateObject();

    cJSON *accounts = db_query(app,
        "SELECT a.id, a.name, a.code, a.type, "
        "COALESCE(SUM(l.debit), 0) as total_debit, "
        "COALESCE(SUM(l.credit), 0) as total_credit, "
        "COALESCE(SUM(l.debit - l.credit), 0) as balance "
        "FROM accounts a LEFT JOIN journal_lines l ON a.id = l.account_id "
        "GROUP BY a.id "
        "HAVING total_debit != 0 OR total_credit != 0 "
        "ORDER BY a.code;");
    cJSON_AddItemToObject(res, "accounts", accounts);

    // Verification: Sum of all debits should equal sum of all credits
    cJSON *verify = db_query(app,
        "SELECT SUM(debit) as total_debit, SUM(credit) as total_credit, "
        "SUM(debit) - SUM(credit) as difference FROM journal_lines;");
    if (verify && cJSON_GetArraySize(verify) > 0) {
        cJSON *v = cJSON_DetachItemFromArray(verify, 0);
        cJSON_AddItemToObject(res, "verification", v);
    }
    cJSON_Delete(verify);

    send_json(c, 200, res);
}

// ============================================================
// CASH FLOW STATEMENT
// ============================================================
void route_report_cash_flow(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    cJSON *res = cJSON_CreateObject();

    // Cash & Bank movements
    cJSON *cash_flow = db_query(app,
        "SELECT e.entry_date, e.memo, l.debit, l.credit, "
        "(l.debit - l.credit) as net_flow "
        "FROM journal_lines l "
        "JOIN journal_entries e ON l.journal_id = e.id "
        "JOIN accounts a ON l.account_id = a.id "
        "WHERE a.id IN (1, 2) "  // Cash + Bank accounts
        "ORDER BY e.entry_date DESC LIMIT 100;");
    cJSON_AddItemToObject(res, "movements", cash_flow);

    // Summary
    cJSON *summary = db_query(app,
        "SELECT "
        "COALESCE(SUM(CASE WHEN l.debit > 0 THEN l.debit ELSE 0 END), 0) as total_inflow, "
        "COALESCE(SUM(CASE WHEN l.credit > 0 THEN l.credit ELSE 0 END), 0) as total_outflow, "
        "COALESCE(SUM(l.debit - l.credit), 0) as net_cash "
        "FROM journal_lines l "
        "JOIN accounts a ON l.account_id = a.id "
        "WHERE a.id IN (1, 2);");
    if (summary && cJSON_GetArraySize(summary) > 0) {
        cJSON *s = cJSON_DetachItemFromArray(summary, 0);
        cJSON_AddItemToObject(res, "summary", s);
    }
    cJSON_Delete(summary);

    send_json(c, 200, res);
}

// ============================================================
// AGING REPORT — Outstanding invoices by age
// ============================================================
void route_report_aging(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    int64_t now = (int64_t)time(NULL);

    // Parameterized aging query — binds 'now' as integer (not string-interpolated)
    cJSON *result = cJSON_CreateArray();
    {
        sqlite3_stmt *stmt;
        const char *sql =
            "SELECT i.invoice_num, c.display_name as customer, "
            "i.total, i.amount_paid, i.balance_due, i.date, "
            "CASE "
            "  WHEN (? - i.date) / 86400 <= 30 THEN 'current' "
            "  WHEN (? - i.date) / 86400 <= 60 THEN '31-60 days' "
            "  WHEN (? - i.date) / 86400 <= 90 THEN '61-90 days' "
            "  ELSE '90+ days' "
            "END as aging_bucket "
            "FROM invoices i "
            "LEFT JOIN contacts c ON i.customer_id = c.id "
            "WHERE i.status NOT IN ('paid', 'void') AND i.balance_due > 0 "
            "ORDER BY i.date ASC;";
        if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int64(stmt, 1, now);
            sqlite3_bind_int64(stmt, 2, now);
            sqlite3_bind_int64(stmt, 3, now);
            int cols = sqlite3_column_count(stmt);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                cJSON *row = cJSON_CreateObject();
                for (int i = 0; i < cols; i++) {
                    const char *name = sqlite3_column_name(stmt, i);
                    switch (sqlite3_column_type(stmt, i)) {
                        case SQLITE_INTEGER: cJSON_AddNumberToObject(row, name, sqlite3_column_int64(stmt, i)); break;
                        case SQLITE_FLOAT:   cJSON_AddNumberToObject(row, name, sqlite3_column_double(stmt, i)); break;
                        case SQLITE_TEXT:    cJSON_AddStringToObject(row, name, (const char*)sqlite3_column_text(stmt, i)); break;
                        default:             cJSON_AddNullToObject(row, name); break;
                    }
                }
                cJSON_AddItemToArray(result, row);
            }
            sqlite3_finalize(stmt);
        }
    }

    // Bucket summary — also parameterized
    cJSON *bucket_obj = cJSON_CreateObject();
    {
        sqlite3_stmt *stmt;
        const char *sum_sql =
            "SELECT "
            "SUM(CASE WHEN (? - date) / 86400 <= 30 THEN balance_due ELSE 0 END) as current_amt, "
            "SUM(CASE WHEN (? - date) / 86400 BETWEEN 31 AND 60 THEN balance_due ELSE 0 END) as days_31_60, "
            "SUM(CASE WHEN (? - date) / 86400 BETWEEN 61 AND 90 THEN balance_due ELSE 0 END) as days_61_90, "
            "SUM(CASE WHEN (? - date) / 86400 > 90 THEN balance_due ELSE 0 END) as days_90_plus "
            "FROM invoices WHERE status NOT IN ('paid','void') AND balance_due > 0;";
        if (sqlite3_prepare_v2(app->db, sum_sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int64(stmt, 1, now);
            sqlite3_bind_int64(stmt, 2, now);
            sqlite3_bind_int64(stmt, 3, now);
            sqlite3_bind_int64(stmt, 4, now);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                int cols = sqlite3_column_count(stmt);
                for (int i = 0; i < cols; i++) {
                    const char *name = sqlite3_column_name(stmt, i);
                    cJSON_AddNumberToObject(bucket_obj, name, sqlite3_column_int64(stmt, i));
                }
            }
            sqlite3_finalize(stmt);
        }
    }

    cJSON *res = cJSON_CreateObject();
    cJSON_AddItemToObject(res, "invoices", result);
    cJSON_AddItemToObject(res, "summary", bucket_obj);
    send_json(c, 200, res);
}

// ============================================================
// SALES BY CUSTOMER
// ============================================================
void route_report_sales_by_customer(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    cJSON *list = db_query(app,
        "SELECT c.display_name as customer, "
        "COUNT(i.id) as invoice_count, "
        "SUM(i.total) as total_sales, "
        "SUM(i.amount_paid) as total_paid, "
        "SUM(i.balance_due) as total_outstanding "
        "FROM invoices i "
        "JOIN contacts c ON i.customer_id = c.id "
        "GROUP BY c.id "
        "ORDER BY total_sales DESC;");
    send_json(c, 200, list);
}

// ============================================================
// EXPENSE BY CATEGORY
// ============================================================
void route_report_expense_by_category(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    cJSON *list = db_query(app,
        "SELECT a.name as category, a.code, "
        "COALESCE(SUM(l.debit), 0) as total_amount, "
        "COUNT(DISTINCT l.journal_id) as entry_count "
        "FROM journal_lines l "
        "JOIN accounts a ON l.account_id = a.id "
        "WHERE a.type = 5 AND l.debit > 0 "
        "GROUP BY a.id "
        "ORDER BY total_amount DESC;");
    send_json(c, 200, list);
}

// ============================================================
// GST SUMMARY (for GSTR-1 preparation)
// ============================================================
void route_report_gst_summary(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    cJSON *res = cJSON_CreateObject();

    // Outward supplies (invoices)
    cJSON *outward = db_query(app,
        "SELECT c.gstin as customer_gstin, c.display_name as customer, c.billing_state, "
        "i.invoice_num, i.date, i.subtotal, i.tax, i.total "
        "FROM invoices i "
        "LEFT JOIN contacts c ON i.customer_id = c.id "
        "WHERE i.status != 'void' "
        "ORDER BY i.date;");
    cJSON_AddItemToObject(res, "outward_supplies", outward);

    // Inward supplies (bills)
    cJSON *inward = db_query(app,
        "SELECT c.gstin as vendor_gstin, c.display_name as vendor, "
        "b.bill_num, b.date, b.subtotal, b.tax, b.total "
        "FROM bills b "
        "LEFT JOIN contacts c ON b.vendor_id = c.id "
        "WHERE b.status != 'void' "
        "ORDER BY b.date;");
    cJSON_AddItemToObject(res, "inward_supplies", inward);

    // Tax liability summary
    cJSON *tax_summary = db_query(app,
        "SELECT "
        "COALESCE((SELECT SUM(tax) FROM invoices WHERE status != 'void'), 0) as output_tax, "
        "COALESCE((SELECT SUM(tax) FROM bills WHERE status != 'void'), 0) as input_tax;");
    if (tax_summary && cJSON_GetArraySize(tax_summary) > 0) {
        cJSON *ts = cJSON_DetachItemFromArray(tax_summary, 0);
        cJSON_AddItemToObject(res, "tax_summary", ts);
    }
    cJSON_Delete(tax_summary);

    send_json(c, 200, res);
}
