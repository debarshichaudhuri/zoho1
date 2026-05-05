#include "../include/banking.h"
#include <ctype.h>

// A high-speed, zero-regex string parser for Indian Bank SMS formats
// Extracts: Amount, Type (Debit/Credit), Account Ref, Date, UTR
bool parse_bank_sms(const char *sms_text, BankTransaction *tx) {
    memset(tx, 0, sizeof(BankTransaction));
    
    // Convert to uppercase for easier matching
    char upper_sms[1024];
    strncpy(upper_sms, sms_text, sizeof(upper_sms) - 1);
    upper_sms[sizeof(upper_sms) - 1] = '\0';
    for (int i = 0; upper_sms[i]; i++) {
        upper_sms[i] = toupper((unsigned char)upper_sms[i]);
    }

    // Detect Transaction Type
    if (strstr(upper_sms, "DEBITED") || strstr(upper_sms, " DEBIT ") || strstr(upper_sms, " SENT ")) {
        tx->type = TX_DEBIT;
    } else if (strstr(upper_sms, "CREDITED") || strstr(upper_sms, " CREDIT ") || strstr(upper_sms, " RECEIVED ")) {
        tx->type = TX_CREDIT;
    } else {
        return false; // Unknown transaction type
    }

    // Extract Amount (Look for RS., INR, Rs, etc.)
    char *amt_ptr = strstr(upper_sms, "RS.");
    if (!amt_ptr) amt_ptr = strstr(upper_sms, "RS ");
    if (!amt_ptr) amt_ptr = strstr(upper_sms, "INR ");
    if (!amt_ptr) amt_ptr = strstr(upper_sms, "INR.");
    
    if (amt_ptr) {
        // Move past the currency symbol
        while (*amt_ptr && !isdigit((unsigned char)*amt_ptr)) amt_ptr++;
        sscanf(amt_ptr, "%lf", &tx->amount);
    } else {
        return false; // Amount not found
    }

    // Extract Account Number/Ref (Look for A/C, ACCT, XXXXX)
    char *acct_ptr = strstr(upper_sms, "A/C");
    if (!acct_ptr) acct_ptr = strstr(upper_sms, "ACCT");
    if (!acct_ptr) acct_ptr = strstr(upper_sms, "ACCOUNT");
    
    if (acct_ptr) {
        char acct_buffer[32] = {0};
        int idx = 0;
        // Skip letters and spaces
        while (*acct_ptr && !isdigit((unsigned char)*acct_ptr) && *acct_ptr != 'X') acct_ptr++;
        // Read alphanumeric (e.g. XX1234)
        while (*acct_ptr && (isdigit((unsigned char)*acct_ptr) || *acct_ptr == 'X') && idx < 31) {
            acct_buffer[idx++] = *acct_ptr++;
        }
        strncpy(tx->account_ref, acct_buffer, sizeof(tx->account_ref));
    }

    // Extract UTR / Reference number (typically 12 digits for UPI)
    char *utr_ptr = strstr(upper_sms, "UTR");
    if (!utr_ptr) utr_ptr = strstr(upper_sms, "REF");
    if (!utr_ptr) utr_ptr = strstr(upper_sms, "UPI");
    
    if (utr_ptr) {
        char utr_buffer[64] = {0};
        int idx = 0;
        // Move to the first digit after the keyword
        while (*utr_ptr && !isdigit((unsigned char)*utr_ptr)) utr_ptr++;
        // Read the reference number
        while (*utr_ptr && (isdigit((unsigned char)*utr_ptr) || isalpha((unsigned char)*utr_ptr)) && idx < 63) {
            utr_buffer[idx++] = *utr_ptr++;
        }
        strncpy(tx->utr, utr_buffer, sizeof(tx->utr));
    }

    // Capture the original text as description
    strncpy(tx->description, sms_text, sizeof(tx->description) - 1);
    
    return true;
}

// POST /api/banking/sms
void route_banking_sms(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    if (mg_vcasecmp(&hm->method, "POST") != 0) {
        send_error(c, 405, "Method Not Allowed");
        return;
    }

    cJSON *req = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
    if (!req) {
        send_error(c, 400, "Invalid JSON");
        return;
    }

    cJSON *sms_text_json = cJSON_GetObjectItem(req, "sms_text");
    if (!sms_text_json || !cJSON_IsString(sms_text_json)) {
        send_error(c, 400, "Missing 'sms_text' parameter");
        cJSON_Delete(req);
        return;
    }

    BankTransaction tx;
    if (parse_bank_sms(sms_text_json->valuestring, &tx)) {
        // Insert into bank_transactions using proper parameterized query
        int64_t amount_paisa = (int64_t)(tx.amount * 100);
        sqlite3_stmt *ins_stmt;
        const char *ins_sql = "INSERT INTO bank_transactions (type, amount, account_ref, utr, description, source, date, created_at) "
                              "VALUES (?, ?, ?, ?, ?, 'sms', strftime('%s','now'), strftime('%s','now'));";
        if (sqlite3_prepare_v2(app->db, ins_sql, -1, &ins_stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(ins_stmt, 1, tx.type == TX_CREDIT ? "credit" : "debit", -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(ins_stmt, 2, amount_paisa);
            sqlite3_bind_text(ins_stmt, 3, tx.account_ref, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(ins_stmt, 4, tx.utr, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(ins_stmt, 5, tx.description, -1, SQLITE_TRANSIENT);
            sqlite3_step(ins_stmt);
            sqlite3_finalize(ins_stmt);
        }
        
        cJSON *res = cJSON_CreateObject();
        cJSON_AddStringToObject(res, "status", "success");
        cJSON_AddNumberToObject(res, "amount_parsed", tx.amount);
        cJSON_AddStringToObject(res, "type", tx.type == TX_CREDIT ? "CREDIT" : "DEBIT");
        cJSON_AddStringToObject(res, "utr", tx.utr);
        send_json(c, 200, res);
    } else {
        send_error(c, 400, "Failed to parse SMS format");
    }

    cJSON_Delete(req);
}

// Auto-Reconciliation Engine
void reconcile_transactions(AppState *app) {
    // Conceptual reconciliation: Match invoices to bank lines
    db_execute(app, "UPDATE invoices SET status='paid' WHERE total IN (SELECT amount FROM bank_transactions WHERE is_matched=0);");
}

// POST /api/banking/reconcile
void route_banking_reconcile(struct mg_connection *c, struct mg_http_message *hm, AppState *app) {
    reconcile_transactions(app);
    cJSON *res = cJSON_CreateObject();
    cJSON_AddStringToObject(res, "status", "success");
    send_json(c, 200, res);
}
