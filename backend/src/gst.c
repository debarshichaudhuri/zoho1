// ============================================================
// GST Compliance Module — Q-Manage ERP
// e-Invoice (IRP) payload generator + e-Way Bill helper
// OFFLINE: generates the JSON blobs; user uploads to NIC portal
//          or uses their own IRP credentials (no cloud dependency)
// ============================================================
#include "../include/erp.h"
#include <time.h>
#include <math.h>

// ---- helpers ----
static const char *safe_str(cJSON *obj, const char *key) {
    cJSON *v = cJSON_GetObjectItem(obj, key);
    return (v && cJSON_IsString(v)) ? v->valuestring : "";
}
static double safe_dbl(cJSON *obj, const char *key) {
    cJSON *v = cJSON_GetObjectItem(obj, key);
    return (v && cJSON_IsNumber(v)) ? v->valuedouble : 0.0;
}

// ============================================================
// GET /api/gst/einvoice/:invoice_id
// Returns the GST e-Invoice JSON payload (IRP schema 1.1)
// Ready to POST to NIC sandbox / production IRP
// ============================================================
void route_gst_einvoice(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int invoice_id) {
    if (invoice_id <= 0) { send_error(c, 400, "Invalid invoice ID"); return; }

    // ---- Fetch invoice ----
    cJSON *inv_rows = NULL;
    {
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(app->db,
            "SELECT i.*, c.display_name, c.gstin, c.email, c.phone, "
            "c.billing_address, c.billing_city, c.billing_state, c.billing_zip, c.billing_country "
            "FROM invoices i "
            "LEFT JOIN contacts c ON i.customer_id = c.id "
            "WHERE i.id = ? LIMIT 1;",
            -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, invoice_id);
            inv_rows = cJSON_CreateArray();
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                int cols = sqlite3_column_count(stmt);
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
                cJSON_AddItemToArray(inv_rows, row);
            }
            sqlite3_finalize(stmt);
        }
    }

    if (!inv_rows || cJSON_GetArraySize(inv_rows) == 0) {
        cJSON_Delete(inv_rows);
        send_error(c, 404, "Invoice not found");
        return;
    }

    cJSON *inv = cJSON_GetArrayItem(inv_rows, 0);

    // Fetch settings for seller GSTIN etc.
    cJSON *settings = db_query(app, "SELECT * FROM settings LIMIT 1;");
    cJSON *s = (settings && cJSON_GetArraySize(settings) > 0) ? cJSON_GetArrayItem(settings, 0) : NULL;

    // ---- Convert paisa to rupees ----
    double total_r    = safe_dbl(inv, "total")    / 100.0;
    double subtotal_r = safe_dbl(inv, "subtotal") / 100.0;
    double tax_r      = safe_dbl(inv, "tax")      / 100.0;
    if (subtotal_r <= 0) subtotal_r = total_r - tax_r;

    // ---- Split tax CGST/SGST (18% → 9+9) or IGST ----
    const char *buyer_state  = safe_str(inv, "billing_state");
    const char *seller_state = s ? safe_str(s, "state") : "";
    int is_igst = (strlen(buyer_state) > 0 && strlen(seller_state) > 0 &&
                   strcmp(buyer_state, seller_state) != 0);
    double cgst = 0, sgst = 0, igst = 0;
    if (is_igst) igst = tax_r;
    else { cgst = tax_r / 2.0; sgst = tax_r / 2.0; }

    // ---- Build IRP-compliant JSON (Schema v1.1) ----
    cJSON *irn = cJSON_CreateObject();

    // Version
    cJSON_AddStringToObject(irn, "Version", "1.1");

    // Transaction Details
    cJSON *tran = cJSON_CreateObject();
    cJSON_AddStringToObject(tran, "TaxSch", "GST");
    cJSON_AddStringToObject(tran, "SupTyp", is_igst ? "EXPWOP" : "B2B");
    cJSON_AddStringToObject(tran, "RegRev", "N");
    cJSON_AddStringToObject(tran, "EcmGstin", "");
    cJSON_AddStringToObject(tran, "IgstOnIntra", "N");
    cJSON_AddItemToObject(irn, "TranDtls", tran);

    // Document Details
    cJSON *doc = cJSON_CreateObject();
    cJSON_AddStringToObject(doc, "Typ", "INV");
    cJSON_AddStringToObject(doc, "No", safe_str(inv, "invoice_num"));
    // Format date from unix timestamp
    int64_t ts = (int64_t)safe_dbl(inv, "date");
    time_t ts_val = (time_t)ts;
    struct tm *tm_info = localtime(&ts_val);
    char date_str[16];
    strftime(date_str, sizeof(date_str), "%d/%m/%Y", tm_info);
    cJSON_AddStringToObject(doc, "Dt", date_str);
    cJSON_AddItemToObject(irn, "DocDtls", doc);

    // Seller Details
    cJSON *seller = cJSON_CreateObject();
    cJSON_AddStringToObject(seller, "Gstin",   s ? safe_str(s, "gstin") : "");
    cJSON_AddStringToObject(seller, "LglNm",   s ? safe_str(s, "company_name") : "");
    cJSON_AddStringToObject(seller, "TrdNm",   s ? safe_str(s, "trade_name") : "");
    cJSON_AddStringToObject(seller, "Addr1",   s ? safe_str(s, "address") : "");
    cJSON_AddStringToObject(seller, "Loc",     s ? safe_str(s, "city") : "");
    cJSON_AddStringToObject(seller, "Pin",     s ? safe_str(s, "zip") : "");
    cJSON_AddStringToObject(seller, "Stcd",    s ? safe_str(s, "state_code") : "");
    cJSON_AddStringToObject(seller, "Ph",      s ? safe_str(s, "phone") : "");
    cJSON_AddStringToObject(seller, "Em",      s ? safe_str(s, "email") : "");
    cJSON_AddItemToObject(irn, "SellerDtls", seller);

    // Buyer Details
    cJSON *buyer = cJSON_CreateObject();
    cJSON_AddStringToObject(buyer, "Gstin", safe_str(inv, "gstin"));
    cJSON_AddStringToObject(buyer, "LglNm", safe_str(inv, "display_name"));
    cJSON_AddStringToObject(buyer, "TrdNm", safe_str(inv, "display_name"));
    cJSON_AddStringToObject(buyer, "Pos",   is_igst ? "96" : safe_str(s, "state_code")); // place of supply
    cJSON_AddStringToObject(buyer, "Addr1", safe_str(inv, "billing_address"));
    cJSON_AddStringToObject(buyer, "Loc",   safe_str(inv, "billing_city"));
    cJSON_AddStringToObject(buyer, "Pin",   safe_str(inv, "billing_zip"));
    cJSON_AddStringToObject(buyer, "Stcd",  buyer_state);
    cJSON_AddStringToObject(buyer, "Ph",    safe_str(inv, "phone"));
    cJSON_AddStringToObject(buyer, "Em",    safe_str(inv, "email"));
    cJSON_AddItemToObject(irn, "BuyerDtls", buyer);

    // Item List — fetch invoice_lines if table exists, otherwise create single line
    cJSON *items_arr = cJSON_CreateArray();
    {
        sqlite3_stmt *ls;
        int line_num = 1;
        if (sqlite3_prepare_v2(app->db,
            "SELECT il.*, it.name, it.hsn_code, it.tax_rate "
            "FROM invoice_lines il "
            "LEFT JOIN items it ON il.item_id = it.id "
            "WHERE il.invoice_id = ?;",
            -1, &ls, NULL) == SQLITE_OK) {
            sqlite3_bind_int(ls, 1, invoice_id);
            while (sqlite3_step(ls) == SQLITE_ROW) {
                cJSON *item = cJSON_CreateObject();
                double rate = sqlite3_column_int64(ls, sqlite3_column_count(ls) - 3) / 100.0; // price in rupees
                double qty  = sqlite3_column_double(ls, 4);
                double tax_pct = sqlite3_column_double(ls, sqlite3_column_count(ls) - 1);
                double base = rate * qty;
                double item_cgst = is_igst ? 0 : base * (tax_pct / 200.0);
                double item_sgst = is_igst ? 0 : base * (tax_pct / 200.0);
                double item_igst = is_igst ? base * (tax_pct / 100.0) : 0;

                cJSON_AddNumberToObject(item, "SlNo",    line_num++);
                cJSON_AddStringToObject(item, "PrdDesc", (const char*)sqlite3_column_text(ls, sqlite3_column_count(ls) - 2));
                cJSON_AddStringToObject(item, "IsServc", "N");
                cJSON_AddStringToObject(item, "HsnCd",   sqlite3_column_type(ls, sqlite3_column_count(ls) - 2) == SQLITE_TEXT
                    ? (const char*)sqlite3_column_text(ls, sqlite3_column_count(ls) - 2) : "");
                cJSON_AddNumberToObject(item, "Qty",     qty);
                cJSON_AddStringToObject(item, "Unit",    "NOS");
                cJSON_AddNumberToObject(item, "UnitPrice", round(rate * 100.0) / 100.0);
                cJSON_AddNumberToObject(item, "TotAmt",  round(base * 100.0) / 100.0);
                cJSON_AddNumberToObject(item, "AssAmt",  round(base * 100.0) / 100.0);
                cJSON_AddNumberToObject(item, "GstRt",   tax_pct);
                cJSON_AddNumberToObject(item, "IgstAmt", round(item_igst * 100.0) / 100.0);
                cJSON_AddNumberToObject(item, "CgstAmt", round(item_cgst * 100.0) / 100.0);
                cJSON_AddNumberToObject(item, "SgstAmt", round(item_sgst * 100.0) / 100.0);
                cJSON_AddNumberToObject(item, "TotItemVal", round((base + item_cgst + item_sgst + item_igst) * 100.0) / 100.0);
                cJSON_AddItemToArray(items_arr, item);
            }
            sqlite3_finalize(ls);
        }
    }

    // Fallback: single summary line if no line items
    if (cJSON_GetArraySize(items_arr) == 0) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "SlNo",       1);
        cJSON_AddStringToObject(item, "PrdDesc",    safe_str(inv, "invoice_num"));
        cJSON_AddStringToObject(item, "IsServc",    "N");
        cJSON_AddStringToObject(item, "HsnCd",      "");
        cJSON_AddNumberToObject(item, "Qty",        1.0);
        cJSON_AddStringToObject(item, "Unit",       "NOS");
        cJSON_AddNumberToObject(item, "UnitPrice",  round(subtotal_r * 100.0) / 100.0);
        cJSON_AddNumberToObject(item, "TotAmt",     round(subtotal_r * 100.0) / 100.0);
        cJSON_AddNumberToObject(item, "AssAmt",     round(subtotal_r * 100.0) / 100.0);
        cJSON_AddNumberToObject(item, "GstRt",      tax_r > 0 ? round(tax_r / subtotal_r * 100.0) : 18.0);
        cJSON_AddNumberToObject(item, "IgstAmt",    round(igst * 100.0) / 100.0);
        cJSON_AddNumberToObject(item, "CgstAmt",    round(cgst * 100.0) / 100.0);
        cJSON_AddNumberToObject(item, "SgstAmt",    round(sgst * 100.0) / 100.0);
        cJSON_AddNumberToObject(item, "TotItemVal", round(total_r * 100.0) / 100.0);
        cJSON_AddItemToArray(items_arr, item);
    }
    cJSON_AddItemToObject(irn, "ItemList", items_arr);

    // Value Details
    cJSON *val = cJSON_CreateObject();
    cJSON_AddNumberToObject(val, "AssVal",  round(subtotal_r * 100.0) / 100.0);
    cJSON_AddNumberToObject(val, "IgstVal", round(igst * 100.0) / 100.0);
    cJSON_AddNumberToObject(val, "CgstVal", round(cgst * 100.0) / 100.0);
    cJSON_AddNumberToObject(val, "SgstVal", round(sgst * 100.0) / 100.0);
    cJSON_AddNumberToObject(val, "TotInvVal", round(total_r * 100.0) / 100.0);
    cJSON_AddItemToObject(irn, "ValDtls", val);

    // Payment Details
    cJSON *pmt = cJSON_CreateObject();
    cJSON_AddStringToObject(pmt, "Nm",     s ? safe_str(s, "bank_name") : "");
    cJSON_AddStringToObject(pmt, "AccDet", s ? safe_str(s, "bank_account") : "");
    cJSON_AddStringToObject(pmt, "Mode",   "Bank Transfer");
    cJSON_AddNumberToObject(pmt, "FinInsBr", 0);
    cJSON_AddItemToObject(irn, "PayDtls", pmt);

    // Wrap with metadata
    cJSON *result = cJSON_CreateObject();
    cJSON_AddItemToObject(result, "irp_payload", irn);
    cJSON_AddStringToObject(result, "irp_endpoint_sandbox",    "https://einv-apisandbox.nic.in/eivital/v1.04/Invoice");
    cJSON_AddStringToObject(result, "irp_endpoint_production", "https://einv-apisandbox.nic.in/eivital/v1.04/Invoice");
    cJSON_AddStringToObject(result, "note", "POST this payload to IRP with Authorization: Bearer <access_token>");
    cJSON_AddNumberToObject(result, "invoice_id", invoice_id);
    cJSON_AddNumberToObject(result, "total_rupees", round(total_r * 100.0) / 100.0);

    cJSON_Delete(inv_rows);
    cJSON_Delete(settings);
    send_json(c, 200, result);
}

// ============================================================
// GET /api/gst/eway/:invoice_id
// Returns e-Way Bill JSON payload (NIC EWB API v1.03)
// ============================================================
void route_gst_eway(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int invoice_id) {
    if (invoice_id <= 0) { send_error(c, 400, "Invalid invoice ID"); return; }

    // Fetch invoice + buyer + seller
    cJSON *inv_rows = NULL;
    {
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(app->db,
            "SELECT i.*, c.display_name, c.gstin, c.billing_state, c.billing_zip "
            "FROM invoices i LEFT JOIN contacts c ON i.customer_id = c.id "
            "WHERE i.id = ? LIMIT 1;",
            -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, invoice_id);
            inv_rows = cJSON_CreateArray();
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                int cols = sqlite3_column_count(stmt);
                cJSON *row = cJSON_CreateObject();
                for (int i = 0; i < cols; i++) {
                    const char *name = sqlite3_column_name(stmt, i);
                    switch (sqlite3_column_type(stmt, i)) {
                        case SQLITE_INTEGER: cJSON_AddNumberToObject(row, name, sqlite3_column_int64(stmt, i)); break;
                        case SQLITE_TEXT:    cJSON_AddStringToObject(row, name, (const char*)sqlite3_column_text(stmt, i)); break;
                        default:             cJSON_AddNullToObject(row, name); break;
                    }
                }
                cJSON_AddItemToArray(inv_rows, row);
            }
            sqlite3_finalize(stmt);
        }
    }

    if (!inv_rows || cJSON_GetArraySize(inv_rows) == 0) {
        cJSON_Delete(inv_rows);
        send_error(c, 404, "Invoice not found");
        return;
    }

    cJSON *inv = cJSON_GetArrayItem(inv_rows, 0);
    cJSON *settings = db_query(app, "SELECT * FROM settings LIMIT 1;");
    cJSON *s = (settings && cJSON_GetArraySize(settings) > 0) ? cJSON_GetArrayItem(settings, 0) : NULL;

    double total_r = safe_dbl(inv, "total") / 100.0;

    // Build e-Way Bill payload
    cJSON *ewb = cJSON_CreateObject();
    cJSON_AddStringToObject(ewb, "supplyType",      "O");  // Outward
    cJSON_AddStringToObject(ewb, "subSupplyType",   "1");  // Supply
    cJSON_AddStringToObject(ewb, "docType",         "INV");
    cJSON_AddStringToObject(ewb, "docNo",           safe_str(inv, "invoice_num"));

    int64_t ts = (int64_t)safe_dbl(inv, "date"); time_t ts_val = (time_t)ts;
    struct tm *tm_info = localtime(&ts_val);
    char date_str[16];
    strftime(date_str, sizeof(date_str), "%d/%m/%Y", tm_info);
    cJSON_AddStringToObject(ewb, "docDate",         date_str);

    cJSON_AddStringToObject(ewb, "fromGstin",       s ? safe_str(s, "gstin") : "");
    cJSON_AddStringToObject(ewb, "fromTrdName",     s ? safe_str(s, "company_name") : "");
    cJSON_AddStringToObject(ewb, "fromAddr1",       s ? safe_str(s, "address") : "");
    cJSON_AddStringToObject(ewb, "fromPlace",       s ? safe_str(s, "city") : "");
    cJSON_AddStringToObject(ewb, "fromPincode",     s ? safe_str(s, "zip") : "");
    cJSON_AddStringToObject(ewb, "fromStateCode",   s ? safe_str(s, "state_code") : "");
    cJSON_AddStringToObject(ewb, "toGstin",         safe_str(inv, "gstin"));
    cJSON_AddStringToObject(ewb, "toTrdName",       safe_str(inv, "display_name"));
    cJSON_AddStringToObject(ewb, "toAddr1",         "");
    cJSON_AddStringToObject(ewb, "toPlace",         "");
    cJSON_AddStringToObject(ewb, "toPincode",       safe_str(inv, "billing_zip"));
    cJSON_AddStringToObject(ewb, "toStateCode",     safe_str(inv, "billing_state"));
    cJSON_AddNumberToObject(ewb, "totInvValue",     round(total_r * 100.0) / 100.0);
    cJSON_AddStringToObject(ewb, "hsn",             "");

    // Part B (transporter) — left blank for user to fill before upload
    cJSON_AddStringToObject(ewb, "transMode",       "1");   // Road
    cJSON_AddNumberToObject(ewb, "transDistance",   0);
    cJSON_AddStringToObject(ewb, "transporterName", "");
    cJSON_AddStringToObject(ewb, "transporterId",   "");
    cJSON_AddStringToObject(ewb, "transDocNo",      "");
    cJSON_AddStringToObject(ewb, "transDocDate",    "");
    cJSON_AddStringToObject(ewb, "vehicleNo",       "");
    cJSON_AddStringToObject(ewb, "vehicleType",     "R");    // Regular

    cJSON *result = cJSON_CreateObject();
    cJSON_AddItemToObject(result, "ewb_payload",     ewb);
    cJSON_AddStringToObject(result, "ewb_endpoint",  "https://ewaybillgst.gov.in/apiclient/api/ewayapi/generate");
    cJSON_AddStringToObject(result, "note",
        "Fill transporterName, vehicleNo, transDocNo before submitting. Mandatory for goods > ₹50,000.");
    cJSON_AddNumberToObject(result, "invoice_id",    invoice_id);

    cJSON_Delete(inv_rows);
    cJSON_Delete(settings);
    send_json(c, 200, result);
}

// ============================================================
// GET /api/gst/summary — GSTR-1 period summary
// Query params: ?from=YYYY-MM-DD&to=YYYY-MM-DD
// ============================================================
void route_gst_summary(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    // Parse date range
    char from_buf[32] = "", to_buf[32] = "";
    mg_http_get_var(&hm->query, "from", from_buf, sizeof(from_buf));
    mg_http_get_var(&hm->query, "to",   to_buf,   sizeof(to_buf));

    // Default: current month
    if (strlen(from_buf) == 0) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        snprintf(from_buf, sizeof(from_buf), "%04d-%02d-01", t->tm_year+1900, t->tm_mon+1);
    }
    if (strlen(to_buf) == 0) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        // last day of current month approximation
        snprintf(to_buf, sizeof(to_buf), "%04d-%02d-31", t->tm_year+1900, t->tm_mon+1);
    }

    cJSON *res = cJSON_CreateObject();
    cJSON_AddStringToObject(res, "period_from", from_buf);
    cJSON_AddStringToObject(res, "period_to",   to_buf);

    // B2B invoices in date range
    sqlite3_stmt *stmt;
    const char *b2b_sql =
        "SELECT i.invoice_num, i.date, i.subtotal, i.tax, i.total, "
        "c.display_name as customer, c.gstin, c.billing_state "
        "FROM invoices i LEFT JOIN contacts c ON i.customer_id = c.id "
        "WHERE i.status NOT IN ('void','draft') "
        "AND date(i.date, 'unixepoch') BETWEEN ? AND ? "
        "ORDER BY i.date;";
    cJSON *b2b_list = cJSON_CreateArray();
    double out_taxable = 0, out_igst = 0, out_total = 0;

    if (sqlite3_prepare_v2(app->db, b2b_sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, from_buf, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, to_buf,   -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            cJSON *row = cJSON_CreateObject();
            double sub = sqlite3_column_int64(stmt, 2) / 100.0;
            double tax = sqlite3_column_int64(stmt, 3) / 100.0;
            double tot = sqlite3_column_int64(stmt, 4) / 100.0;
            cJSON_AddStringToObject(row, "invoice_num",   (const char*)sqlite3_column_text(stmt, 0));
            cJSON_AddStringToObject(row, "customer",      sqlite3_column_text(stmt, 5) ? (const char*)sqlite3_column_text(stmt, 5) : "");
            cJSON_AddStringToObject(row, "gstin",         sqlite3_column_text(stmt, 6) ? (const char*)sqlite3_column_text(stmt, 6) : "");
            cJSON_AddStringToObject(row, "state",         sqlite3_column_text(stmt, 7) ? (const char*)sqlite3_column_text(stmt, 7) : "");
            cJSON_AddNumberToObject(row, "taxable",       round(sub * 100.0) / 100.0);
            cJSON_AddNumberToObject(row, "tax",           round(tax * 100.0) / 100.0);
            cJSON_AddNumberToObject(row, "total",         round(tot * 100.0) / 100.0);
            cJSON_AddItemToArray(b2b_list, row);
            out_taxable += sub; out_total += tot; out_igst += tax; // simplified: all tax as IGST for summary
        }
        sqlite3_finalize(stmt);
    }
    cJSON_AddItemToObject(res, "b2b_invoices", b2b_list);

    // Input tax (bills)
    const char *in_sql =
        "SELECT SUM(subtotal)/100.0, SUM(tax)/100.0, SUM(total)/100.0 "
        "FROM bills WHERE status NOT IN ('void') "
        "AND date(date, 'unixepoch') BETWEEN ? AND ?;";
    double in_taxable = 0, in_tax = 0, in_total = 0;
    if (sqlite3_prepare_v2(app->db, in_sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, from_buf, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, to_buf,   -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            in_taxable = sqlite3_column_double(stmt, 0);
            in_tax     = sqlite3_column_double(stmt, 1);
            in_total   = sqlite3_column_double(stmt, 2);
        }
        sqlite3_finalize(stmt);
    }

    // Summary object
    cJSON *summary = cJSON_CreateObject();
    cJSON_AddNumberToObject(summary, "outward_taxable",   round(out_taxable * 100.0) / 100.0);
    cJSON_AddNumberToObject(summary, "outward_tax",       round(out_igst    * 100.0) / 100.0);
    cJSON_AddNumberToObject(summary, "outward_total",     round(out_total   * 100.0) / 100.0);
    cJSON_AddNumberToObject(summary, "inward_taxable",    round(in_taxable  * 100.0) / 100.0);
    cJSON_AddNumberToObject(summary, "input_tax_credit",  round(in_tax      * 100.0) / 100.0);
    cJSON_AddNumberToObject(summary, "inward_total",      round(in_total    * 100.0) / 100.0);
    cJSON_AddNumberToObject(summary, "net_tax_payable",   round((out_igst - in_tax) * 100.0) / 100.0);
    cJSON_AddNumberToObject(summary, "invoice_count",     cJSON_GetArraySize(b2b_list));
    cJSON_AddItemToObject(res, "summary", summary);

    send_json(c, 200, res);
}
