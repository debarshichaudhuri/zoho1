#include "../include/migration.h"

// ============================================================
// CSV Parser — Handles quoted fields with embedded commas
// ============================================================
static int parse_csv_line(char *line, char **fields, int max_fields) {
    int count = 0;
    bool in_quotes = false;
    char *p = line;

    if (count < max_fields) {
        fields[count++] = p;
    }

    while (*p) {
        if (*p == '"') {
            in_quotes = !in_quotes;
        } else if (*p == ',' && !in_quotes) {
            *p = '\0';
            if (count < max_fields) {
                fields[count++] = p + 1;
            }
        } else if (*p == '\r' || *p == '\n') {
            *p = '\0';
            break;
        }
        p++;
    }
    return count;
}

// Strip surrounding quotes from a CSV field
static char* strip_quotes(char *s) {
    if (!s) return "";
    while (*s == '"' || *s == ' ') s++;
    int len = (int)strlen(s);
    while (len > 0 && (s[len-1] == '"' || s[len-1] == ' ' || s[len-1] == '\r')) {
        s[--len] = '\0';
    }
    return s;
}

// ============================================================
// Contacts CSV Importer (PARAMETERIZED — SQL Injection Fixed)
// ============================================================
// Zoho format: "Contact Name", "Company Name", "Contact Type", "Email", "Phone", "GSTIN"
int process_zoho_contacts_csv(AppState *app, const char *csv_data) {
    printf("[IMPORT] Processing Zoho Contacts CSV...\n");
    char *data_copy = strdup(csv_data);
    if (!data_copy) return -1;

    char *line = strtok(data_copy, "\n");
    if (!line) { free(data_copy); return 0; }

    // Skip header row
    line = strtok(NULL, "\n");
    int inserted = 0;

    sqlite3_exec(app->db, "BEGIN TRANSACTION;", 0, 0, 0);

    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO contacts (display_name, company_name, type, email, phone, gstin, created_at) "
                      "VALUES (?, ?, ?, ?, ?, ?, strftime('%s','now'));";

    if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        free(data_copy);
        return -1;
    }

    while (line) {
        char *fields[20];
        int num_fields = parse_csv_line(line, fields, 20);

        if (num_fields >= 5) {
            char *name = strip_quotes(fields[0]);
            char *company = strip_quotes(fields[1]);
            char *type = strip_quotes(fields[2]);
            char *email = strip_quotes(fields[3]);
            char *phone = strip_quotes(fields[4]);
            char *gstin = num_fields > 5 ? strip_quotes(fields[5]) : "";

            if (strlen(name) > 0) {
                sqlite3_reset(stmt);
                sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 2, company, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 3, strlen(type) > 0 ? type : "customer", -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 4, email, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 5, phone, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 6, gstin, -1, SQLITE_TRANSIENT);

                if (sqlite3_step(stmt) == SQLITE_DONE) inserted++;
            }
        }
        line = strtok(NULL, "\n");
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(app->db, "COMMIT;", 0, 0, 0);
    free(data_copy);
    printf("[IMPORT] Contacts: %d rows imported\n", inserted);
    return inserted;
}

// ============================================================
// Items CSV Importer (NEW)
// ============================================================
// Format: "Item Name", "SKU", "Rate", "HSN/SAC", "Tax%", "Unit"
int process_zoho_items_csv(AppState *app, const char *csv_data) {
    printf("[IMPORT] Processing Items CSV...\n");
    char *data_copy = strdup(csv_data);
    if (!data_copy) return -1;

    char *line = strtok(data_copy, "\n");
    if (!line) { free(data_copy); return 0; }
    line = strtok(NULL, "\n");
    int inserted = 0;

    sqlite3_exec(app->db, "BEGIN TRANSACTION;", 0, 0, 0);

    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO items (name, sku, price, hsn_code, tax_rate, unit, created_at) "
                      "VALUES (?, ?, ?, ?, ?, ?, strftime('%s','now'));";

    if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        free(data_copy); return -1;
    }

    while (line) {
        char *fields[20];
        int num_fields = parse_csv_line(line, fields, 20);

        if (num_fields >= 3) {
            char *name = strip_quotes(fields[0]);
            char *sku = strip_quotes(fields[1]);
            char *rate_s = strip_quotes(fields[2]);
            char *hsn = num_fields > 3 ? strip_quotes(fields[3]) : "";
            char *tax_s = num_fields > 4 ? strip_quotes(fields[4]) : "18";
            char *unit = num_fields > 5 ? strip_quotes(fields[5]) : "pcs";

            if (strlen(name) > 0) {
                // Convert rupees to paisa
                int64_t price_paisa = (int64_t)(atof(rate_s) * 100);
                double tax_rate = atof(tax_s);

                sqlite3_reset(stmt);
                sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 2, sku, -1, SQLITE_TRANSIENT);
                sqlite3_bind_int64(stmt, 3, price_paisa);
                sqlite3_bind_text(stmt, 4, hsn, -1, SQLITE_TRANSIENT);
                sqlite3_bind_double(stmt, 5, tax_rate);
                sqlite3_bind_text(stmt, 6, unit, -1, SQLITE_TRANSIENT);

                if (sqlite3_step(stmt) == SQLITE_DONE) inserted++;
            }
        }
        line = strtok(NULL, "\n");
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(app->db, "COMMIT;", 0, 0, 0);
    free(data_copy);
    printf("[IMPORT] Items: %d rows imported\n", inserted);
    return inserted;
}

// ============================================================
// Invoices CSV Importer (NEW)
// ============================================================
// Format: "Invoice#", "Customer Name", "Date", "Total", "Status"
int process_zoho_invoices_csv(AppState *app, const char *csv_data) {
    printf("[IMPORT] Processing Invoices CSV...\n");
    char *data_copy = strdup(csv_data);
    if (!data_copy) return -1;

    char *line = strtok(data_copy, "\n");
    if (!line) { free(data_copy); return 0; }
    line = strtok(NULL, "\n");
    int inserted = 0;

    sqlite3_exec(app->db, "BEGIN TRANSACTION;", 0, 0, 0);

    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO invoices (invoice_num, customer_id, total, balance_due, status, date, created_at) "
                      "VALUES (?, ("
                      "  SELECT id FROM contacts WHERE display_name = ? LIMIT 1"
                      "), ?, ?, ?, strftime('%s', ?), strftime('%s','now'));";

    if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        free(data_copy); return -1;
    }

    while (line) {
        char *fields[20];
        int num_fields = parse_csv_line(line, fields, 20);

        if (num_fields >= 4) {
            char *inv_num = strip_quotes(fields[0]);
            char *customer = strip_quotes(fields[1]);
            char *date_s = strip_quotes(fields[2]);
            char *total_s = strip_quotes(fields[3]);
            char *status = num_fields > 4 ? strip_quotes(fields[4]) : "sent";

            if (strlen(inv_num) > 0) {
                int64_t total_paisa = (int64_t)(atof(total_s) * 100);

                // Determine balance based on status
                int64_t balance = total_paisa;
                if (strcmp(status, "paid") == 0 || strcmp(status, "Paid") == 0) balance = 0;

                sqlite3_reset(stmt);
                sqlite3_bind_text(stmt, 1, inv_num, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 2, customer, -1, SQLITE_TRANSIENT);
                sqlite3_bind_int64(stmt, 3, total_paisa);
                sqlite3_bind_int64(stmt, 4, balance);
                sqlite3_bind_text(stmt, 5, status, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 6, date_s, -1, SQLITE_TRANSIENT);

                if (sqlite3_step(stmt) == SQLITE_DONE) inserted++;
            }
        }
        line = strtok(NULL, "\n");
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(app->db, "COMMIT;", 0, 0, 0);
    free(data_copy);
    printf("[IMPORT] Invoices: %d rows imported\n", inserted);
    return inserted;
}

// ============================================================
// Bills CSV Importer (NEW)
// ============================================================
// Format: "Bill#", "Vendor Name", "Date", "Total", "Status"
int process_zoho_bills_csv(AppState *app, const char *csv_data) {
    printf("[IMPORT] Processing Bills CSV...\n");
    char *data_copy = strdup(csv_data);
    if (!data_copy) return -1;

    char *line = strtok(data_copy, "\n");
    if (!line) { free(data_copy); return 0; }
    line = strtok(NULL, "\n");
    int inserted = 0;

    sqlite3_exec(app->db, "BEGIN TRANSACTION;", 0, 0, 0);

    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO bills (bill_num, vendor_id, total, balance_due, status, date, created_at) "
                      "VALUES (?, ("
                      "  SELECT id FROM contacts WHERE display_name = ? LIMIT 1"
                      "), ?, ?, ?, strftime('%s', ?), strftime('%s','now'));";

    if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        free(data_copy); return -1;
    }

    while (line) {
        char *fields[20];
        int num_fields = parse_csv_line(line, fields, 20);

        if (num_fields >= 4) {
            char *bill_num = strip_quotes(fields[0]);
            char *vendor = strip_quotes(fields[1]);
            char *date_s = strip_quotes(fields[2]);
            char *total_s = strip_quotes(fields[3]);
            char *status = num_fields > 4 ? strip_quotes(fields[4]) : "received";

            if (strlen(bill_num) > 0) {
                int64_t total_paisa = (int64_t)(atof(total_s) * 100);
                int64_t balance = total_paisa;
                if (strcmp(status, "paid") == 0 || strcmp(status, "Paid") == 0) balance = 0;

                sqlite3_reset(stmt);
                sqlite3_bind_text(stmt, 1, bill_num, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 2, vendor, -1, SQLITE_TRANSIENT);
                sqlite3_bind_int64(stmt, 3, total_paisa);
                sqlite3_bind_int64(stmt, 4, balance);
                sqlite3_bind_text(stmt, 5, status, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 6, date_s, -1, SQLITE_TRANSIENT);

                if (sqlite3_step(stmt) == SQLITE_DONE) inserted++;
            }
        }
        line = strtok(NULL, "\n");
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(app->db, "COMMIT;", 0, 0, 0);
    free(data_copy);
    printf("[IMPORT] Bills: %d rows imported\n", inserted);
    return inserted;
}

// ============================================================
// POST /api/migration/upload — Multi-Type Import Router
// ============================================================
void route_migration_import(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    if (mg_vcasecmp(&hm->method, "POST") != 0) {
        send_error(c, 405, "Method Not Allowed");
        return;
    }

    struct mg_str *type = mg_http_get_header(hm, "X-Import-Type");
    int rows_inserted = 0;
    const char *type_name = "unknown";

    if (type && mg_vcasecmp(type, "contacts") == 0) {
        rows_inserted = process_zoho_contacts_csv(app, hm->body.buf);
        type_name = "contacts";
    } else if (type && mg_vcasecmp(type, "items") == 0) {
        rows_inserted = process_zoho_items_csv(app, hm->body.buf);
        type_name = "items";
    } else if (type && mg_vcasecmp(type, "invoices") == 0) {
        rows_inserted = process_zoho_invoices_csv(app, hm->body.buf);
        type_name = "invoices";
    } else if (type && mg_vcasecmp(type, "bills") == 0) {
        rows_inserted = process_zoho_bills_csv(app, hm->body.buf);
        type_name = "bills";
    } else {
        send_error(c, 400, "Missing or unsupported X-Import-Type. Supported: contacts, items, invoices, bills");
        return;
    }

    cJSON *res = cJSON_CreateObject();
    cJSON_AddStringToObject(res, "status", rows_inserted >= 0 ? "success" : "error");
    cJSON_AddStringToObject(res, "type", type_name);
    cJSON_AddNumberToObject(res, "rows_imported", rows_inserted);
    send_json(c, rows_inserted >= 0 ? 200 : 500, res);
}
