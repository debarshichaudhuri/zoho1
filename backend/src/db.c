#include "../include/qmanage.h"
#include "../include/crypto.h"
#include <stdarg.h>
#include <math.h>

// ============================================================
// Full Production Schema
// ============================================================
const char *SCHEMA =
    "PRAGMA journal_mode=WAL;"
    "PRAGMA synchronous=NORMAL;"
    "PRAGMA foreign_keys=ON;"
    "PRAGMA cache_size=-8000;"  // 8MB cache

    // ============================================================
    // CORE TABLES
    // ============================================================

    // Organizations
    "CREATE TABLE IF NOT EXISTS organizations ("
    "  id INTEGER PRIMARY KEY,"
    "  name TEXT NOT NULL,"
    "  gstin TEXT, pan TEXT, cin TEXT,"
    "  address TEXT, city TEXT, state TEXT, pincode TEXT,"
    "  phone TEXT, email TEXT, website TEXT,"
    "  base_currency TEXT DEFAULT 'INR',"
    "  fiscal_year_start INTEGER DEFAULT 4,"
    "  logo_path TEXT,"
    "  created_at INTEGER"
    ");"

    // Chart of Accounts (Indian Accounting Standard)
    "CREATE TABLE IF NOT EXISTS accounts ("
    "  id INTEGER PRIMARY KEY,"
    "  name TEXT NOT NULL,"
    "  code TEXT UNIQUE,"
    "  type INTEGER NOT NULL,"  // 1:Asset 2:Liab 3:Equity 4:Income 5:Expense
    "  parent_id INTEGER DEFAULT 0,"
    "  description TEXT,"
    "  is_system INTEGER DEFAULT 0,"
    "  created_at INTEGER"
    ");"

    // Contacts (Unified: Customers + Vendors + Leads)
    "CREATE TABLE IF NOT EXISTS contacts ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  type TEXT CHECK(type IN ('customer','vendor','both','lead')),"
    "  display_name TEXT NOT NULL,"
    "  company_name TEXT,"
    "  email TEXT, phone TEXT, mobile TEXT,"
    "  gstin TEXT, pan TEXT,"
    "  billing_address TEXT, billing_city TEXT, billing_state TEXT, billing_pincode TEXT,"
    "  shipping_address TEXT, shipping_city TEXT, shipping_state TEXT, shipping_pincode TEXT,"
    "  opening_balance INTEGER DEFAULT 0,"  // stored in paisa (÷100 for rupees)
    "  credit_limit INTEGER DEFAULT 0,"
    "  payment_terms INTEGER DEFAULT 30,"  // days
    "  notes TEXT,"
    "  is_active INTEGER DEFAULT 1,"
    "  created_at INTEGER"
    ");"

    // ============================================================
    // DOUBLE-ENTRY LEDGER
    // ============================================================

    "CREATE TABLE IF NOT EXISTS journal_entries ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  entry_date INTEGER NOT NULL,"
    "  memo TEXT,"
    "  source_module TEXT,"  // INVOICE, BILL, PAYMENT, EXPENSE, MANUAL
    "  source_id INTEGER,"
    "  is_finalized INTEGER DEFAULT 0,"
    "  audit_hash TEXT,"  // SHA-256 chain
    "  created_at INTEGER"
    ");"

    "CREATE TABLE IF NOT EXISTS journal_lines ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  journal_id INTEGER NOT NULL,"
    "  account_id INTEGER NOT NULL,"
    "  debit INTEGER DEFAULT 0,"   // paisa
    "  credit INTEGER DEFAULT 0,"  // paisa
    "  memo TEXT,"
    "  FOREIGN KEY(journal_id) REFERENCES journal_entries(id) ON DELETE CASCADE,"
    "  FOREIGN KEY(account_id) REFERENCES accounts(id)"
    ");"

    // ============================================================
    // SALES MODULE (Zoho Books: Sales)
    // ============================================================

    // Items / Products / Services
    "CREATE TABLE IF NOT EXISTS items ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  name TEXT NOT NULL, sku TEXT,"
    "  type TEXT DEFAULT 'product',"  // product, service
    "  hsn_code TEXT,"  // GST HSN/SAC code
    "  unit TEXT DEFAULT 'pcs',"
    "  price INTEGER DEFAULT 0,"     // paisa
    "  cost_price INTEGER DEFAULT 0," // paisa
    "  tax_rate REAL DEFAULT 18.0,"
    "  stock INTEGER DEFAULT 0,"
    "  reorder_level INTEGER DEFAULT 0,"
    "  warehouse_id INTEGER DEFAULT 1,"
    "  is_active INTEGER DEFAULT 1,"
    "  created_at INTEGER"
    ");"

    // Quotes / Estimates
    "CREATE TABLE IF NOT EXISTS quotes ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  quote_num TEXT UNIQUE NOT NULL,"
    "  customer_id INTEGER NOT NULL,"
    "  status TEXT DEFAULT 'draft',"  // draft, sent, accepted, declined, converted
    "  date INTEGER, expiry_date INTEGER,"
    "  subtotal INTEGER DEFAULT 0, tax INTEGER DEFAULT 0, total INTEGER DEFAULT 0,"
    "  notes TEXT, terms TEXT,"
    "  converted_invoice_id INTEGER,"
    "  created_at INTEGER,"
    "  FOREIGN KEY(customer_id) REFERENCES contacts(id)"
    ");"

    "CREATE TABLE IF NOT EXISTS quote_lines ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  quote_id INTEGER NOT NULL,"
    "  item_id INTEGER,"
    "  description TEXT, quantity REAL DEFAULT 1,"
    "  rate INTEGER DEFAULT 0, tax_rate REAL DEFAULT 18.0,"
    "  amount INTEGER DEFAULT 0,"
    "  FOREIGN KEY(quote_id) REFERENCES quotes(id) ON DELETE CASCADE"
    ");"

    // Invoices (enhanced)
    "CREATE TABLE IF NOT EXISTS invoices ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  invoice_num TEXT UNIQUE NOT NULL,"
    "  customer_id INTEGER NOT NULL,"
    "  status TEXT DEFAULT 'draft',"  // draft, sent, viewed, partial, paid, overdue, void
    "  date INTEGER, due_date INTEGER,"
    "  subtotal INTEGER DEFAULT 0, tax INTEGER DEFAULT 0, total INTEGER DEFAULT 0,"
    "  amount_paid INTEGER DEFAULT 0, balance_due INTEGER DEFAULT 0,"
    "  notes TEXT, terms TEXT,"
    "  source_quote_id INTEGER,"
    "  journal_id INTEGER,"
    "  created_at INTEGER,"
    "  FOREIGN KEY(customer_id) REFERENCES contacts(id)"
    ");"

    "CREATE TABLE IF NOT EXISTS invoice_lines ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  invoice_id INTEGER NOT NULL,"
    "  item_id INTEGER,"
    "  description TEXT, quantity REAL DEFAULT 1,"
    "  rate INTEGER DEFAULT 0, tax_rate REAL DEFAULT 18.0,"
    "  amount INTEGER DEFAULT 0,"
    "  FOREIGN KEY(invoice_id) REFERENCES invoices(id) ON DELETE CASCADE"
    ");"

    // Payments Received (against invoices)
    "CREATE TABLE IF NOT EXISTS payments_received ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  payment_num TEXT,"
    "  customer_id INTEGER NOT NULL,"
    "  invoice_id INTEGER,"
    "  amount INTEGER NOT NULL,"  // paisa
    "  date INTEGER,"
    "  mode TEXT DEFAULT 'bank_transfer',"  // cash, bank_transfer, upi, cheque, card
    "  reference TEXT,"  // UTR, cheque no, UPI ref
    "  notes TEXT,"
    "  journal_id INTEGER,"
    "  created_at INTEGER,"
    "  FOREIGN KEY(customer_id) REFERENCES contacts(id),"
    "  FOREIGN KEY(invoice_id) REFERENCES invoices(id)"
    ");"

    // Credit Notes
    "CREATE TABLE IF NOT EXISTS credit_notes ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  cn_num TEXT UNIQUE NOT NULL,"
    "  customer_id INTEGER NOT NULL,"
    "  invoice_id INTEGER,"
    "  amount INTEGER DEFAULT 0,"
    "  reason TEXT, date INTEGER,"
    "  journal_id INTEGER,"
    "  created_at INTEGER,"
    "  FOREIGN KEY(customer_id) REFERENCES contacts(id)"
    ");"

    // ============================================================
    // PURCHASES MODULE (Zoho Books: Purchases)
    // ============================================================

    // Bills (vendor invoices)
    "CREATE TABLE IF NOT EXISTS bills ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  bill_num TEXT NOT NULL,"
    "  vendor_id INTEGER NOT NULL,"
    "  status TEXT DEFAULT 'draft',"  // draft, received, partial, paid, overdue, void
    "  date INTEGER, due_date INTEGER,"
    "  subtotal INTEGER DEFAULT 0, tax INTEGER DEFAULT 0, total INTEGER DEFAULT 0,"
    "  amount_paid INTEGER DEFAULT 0, balance_due INTEGER DEFAULT 0,"
    "  notes TEXT,"
    "  journal_id INTEGER,"
    "  created_at INTEGER,"
    "  FOREIGN KEY(vendor_id) REFERENCES contacts(id)"
    ");"

    "CREATE TABLE IF NOT EXISTS bill_lines ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  bill_id INTEGER NOT NULL,"
    "  item_id INTEGER,"
    "  description TEXT, quantity REAL DEFAULT 1,"
    "  rate INTEGER DEFAULT 0, tax_rate REAL DEFAULT 18.0,"
    "  amount INTEGER DEFAULT 0,"
    "  account_id INTEGER DEFAULT 8,"  // expense account
    "  FOREIGN KEY(bill_id) REFERENCES bills(id) ON DELETE CASCADE"
    ");"

    // Payments Made (against bills)
    "CREATE TABLE IF NOT EXISTS payments_made ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  payment_num TEXT,"
    "  vendor_id INTEGER NOT NULL,"
    "  bill_id INTEGER,"
    "  amount INTEGER NOT NULL,"  // paisa
    "  date INTEGER,"
    "  mode TEXT DEFAULT 'bank_transfer',"
    "  reference TEXT,"
    "  notes TEXT,"
    "  journal_id INTEGER,"
    "  created_at INTEGER,"
    "  FOREIGN KEY(vendor_id) REFERENCES contacts(id),"
    "  FOREIGN KEY(bill_id) REFERENCES bills(id)"
    ");"

    // Purchase Orders
    "CREATE TABLE IF NOT EXISTS purchase_orders ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  po_num TEXT UNIQUE NOT NULL,"
    "  vendor_id INTEGER NOT NULL,"
    "  status TEXT DEFAULT 'draft',"
    "  date INTEGER, expected_date INTEGER,"
    "  total INTEGER DEFAULT 0,"
    "  converted_bill_id INTEGER,"
    "  created_at INTEGER,"
    "  FOREIGN KEY(vendor_id) REFERENCES contacts(id)"
    ");"

    // Vendor Credits
    "CREATE TABLE IF NOT EXISTS vendor_credits ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  vc_num TEXT UNIQUE NOT NULL,"
    "  vendor_id INTEGER NOT NULL,"
    "  bill_id INTEGER,"
    "  amount INTEGER DEFAULT 0,"
    "  reason TEXT, date INTEGER,"
    "  journal_id INTEGER,"
    "  created_at INTEGER,"
    "  FOREIGN KEY(vendor_id) REFERENCES contacts(id)"
    ");"

    // ============================================================
    // CRM MODULE (Replaces Zoho CRM)
    // ============================================================

    // Deals / Opportunities Pipeline
    "CREATE TABLE IF NOT EXISTS deals ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  title TEXT NOT NULL,"
    "  contact_id INTEGER,"
    "  stage TEXT DEFAULT 'contacted',"  // contacted, qualified, quoted, negotiating, won, lost
    "  amount INTEGER DEFAULT 0,"
    "  probability INTEGER DEFAULT 50,"
    "  expected_close INTEGER,"  // date
    "  source TEXT,"  // referral, website, cold_call, walk_in
    "  assigned_to TEXT,"
    "  notes TEXT,"
    "  won_date INTEGER, lost_reason TEXT,"
    "  converted_invoice_id INTEGER,"
    "  created_at INTEGER,"
    "  FOREIGN KEY(contact_id) REFERENCES contacts(id)"
    ");"

    // Activity Log (CRM interactions)
    "CREATE TABLE IF NOT EXISTS activities ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  type TEXT,"  // call, email, meeting, note, task
    "  contact_id INTEGER,"
    "  deal_id INTEGER,"
    "  subject TEXT, description TEXT,"
    "  date INTEGER, completed INTEGER DEFAULT 0,"
    "  created_at INTEGER"
    ");"

    // Leads (CRM pre-deal prospects)
    "CREATE TABLE IF NOT EXISTS leads ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  name TEXT NOT NULL,"
    "  company TEXT,"
    "  email TEXT,"
    "  phone TEXT,"
    "  source TEXT DEFAULT 'website',"
    "  status TEXT DEFAULT 'new',"
    "  assigned_to TEXT,"
    "  notes TEXT,"
    "  contact_id INTEGER,"
    "  deal_id INTEGER,"
    "  created_at INTEGER"
    ");"

    // ============================================================
    // INVENTORY MODULE (Replaces Zoho Inventory)
    // ============================================================

    "CREATE TABLE IF NOT EXISTS warehouses ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  name TEXT NOT NULL,"
    "  address TEXT, is_primary INTEGER DEFAULT 0,"
    "  created_at INTEGER"
    ");"

    "CREATE TABLE IF NOT EXISTS inventory_movements ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  item_id INTEGER NOT NULL,"
    "  warehouse_id INTEGER DEFAULT 1,"
    "  type TEXT,"  // purchase, sale, adjustment, transfer, return
    "  quantity INTEGER,"  // positive = in, negative = out
    "  reference_type TEXT,"  // invoice, bill, adjustment
    "  reference_id INTEGER,"
    "  notes TEXT,"
    "  created_at INTEGER,"
    "  FOREIGN KEY(item_id) REFERENCES items(id)"
    ");"

    // ============================================================
    // BANKING MODULE (Zero-API: Gmail IMAP + SMS)
    // ============================================================

    "CREATE TABLE IF NOT EXISTS bank_accounts ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  name TEXT NOT NULL,"
    "  bank_name TEXT, account_number TEXT, ifsc TEXT,"
    "  account_id INTEGER,"  // linked Chart of Accounts entry
    "  balance INTEGER DEFAULT 0,"
    "  is_primary INTEGER DEFAULT 0,"
    "  created_at INTEGER"
    ");"

    "CREATE TABLE IF NOT EXISTS bank_transactions ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  bank_account_id INTEGER DEFAULT 1,"
    "  type TEXT,"  // credit, debit
    "  amount INTEGER DEFAULT 0,"
    "  account_ref TEXT, utr TEXT,"
    "  description TEXT,"
    "  date INTEGER,"
    "  is_matched INTEGER DEFAULT 0,"
    "  matched_type TEXT,"  // invoice, bill, expense
    "  matched_id INTEGER,"
    "  source TEXT DEFAULT 'manual',"  // manual, sms, gmail
    "  created_at INTEGER"
    ");"

    // Gmail IMAP Configuration (for bank notification parsing)
    "CREATE TABLE IF NOT EXISTS gmail_config ("
    "  id INTEGER PRIMARY KEY,"
    "  email TEXT,"
    "  app_password TEXT,"  // encrypted at rest
    "  imap_server TEXT DEFAULT 'imap.gmail.com',"
    "  last_uid INTEGER DEFAULT 0,"
    "  poll_interval INTEGER DEFAULT 60,"  // seconds
    "  is_active INTEGER DEFAULT 0,"
    "  created_at INTEGER"
    ");"

    // ============================================================
    // BACKUP & SYNC (AES-256 + Google Drive)
    // ============================================================

    "CREATE TABLE IF NOT EXISTS backup_log ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  filename TEXT NOT NULL,"
    "  size_bytes INTEGER,"
    "  encrypted INTEGER DEFAULT 0,"
    "  generation INTEGER DEFAULT 1,"
    "  uploaded_to_drive INTEGER DEFAULT 0,"
    "  drive_file_id TEXT,"
    "  created_at INTEGER"
    ");"

    // Google Drive OAuth Config
    "CREATE TABLE IF NOT EXISTS gdrive_config ("
    "  id INTEGER PRIMARY KEY,"
    "  client_id TEXT, client_secret TEXT,"
    "  refresh_token TEXT, access_token TEXT,"
    "  token_expiry INTEGER,"
    "  folder_id TEXT,"  // Drive folder for backups
    "  max_generations INTEGER DEFAULT 15,"
    "  auto_backup INTEGER DEFAULT 1,"
    "  created_at INTEGER"
    ");"

    // ============================================================
    // AUDIT & COMPLIANCE
    // ============================================================

    // SHA-256 tamper-proof audit trail (Companies Act 2013)
    "CREATE TABLE IF NOT EXISTS audit_trail ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  entity_type TEXT NOT NULL,"  // invoice, bill, payment, journal
    "  entity_id INTEGER NOT NULL,"
    "  action TEXT NOT NULL,"  // create, update, delete, finalize
    "  details TEXT,"  // human-readable audit summary
    "  old_value TEXT,"  // JSON
    "  new_value TEXT,"  // JSON
    "  user_name TEXT DEFAULT 'admin',"
    "  ip_address TEXT,"
    "  hash TEXT,"  // SHA-256 chained hash
    "  prev_hash TEXT,"
    "  created_at INTEGER"
    ");"

    // ============================================================
    // PROJECTS & TIMESHEETS
    // ============================================================

    "CREATE TABLE IF NOT EXISTS projects ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  name TEXT NOT NULL,"
    "  customer_id INTEGER,"
    "  status TEXT DEFAULT 'active',"
    "  budget INTEGER DEFAULT 0,"
    "  start_date INTEGER, end_date INTEGER,"
    "  billing_method TEXT DEFAULT 'fixed',"  // fixed, hourly, task_based
    "  hourly_rate INTEGER DEFAULT 0,"
    "  notes TEXT,"
    "  created_at INTEGER,"
    "  FOREIGN KEY(customer_id) REFERENCES contacts(id)"
    ");"

    "CREATE TABLE IF NOT EXISTS timesheets ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  project_id INTEGER NOT NULL,"
    "  task TEXT,"
    "  hours REAL DEFAULT 0,"
    "  date INTEGER,"
    "  is_billable INTEGER DEFAULT 1,"
    "  is_invoiced INTEGER DEFAULT 0,"
    "  notes TEXT,"
    "  created_at INTEGER,"
    "  FOREIGN KEY(project_id) REFERENCES projects(id)"
    ");"

    // ============================================================
    // AUTHENTICATION (Session Tokens)
    // ============================================================

    "CREATE TABLE IF NOT EXISTS users ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  username TEXT UNIQUE NOT NULL,"
    "  password_hash TEXT NOT NULL,"  // PBKDF2-SHA256 hex
    "  password_salt TEXT NOT NULL,"  // 16-byte hex
    "  role TEXT DEFAULT 'admin',"    // admin, accountant, viewer
    "  is_active INTEGER DEFAULT 1,"
    "  last_login INTEGER,"
    "  created_at INTEGER"
    ");"

    "CREATE TABLE IF NOT EXISTS sessions ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  token TEXT UNIQUE NOT NULL,"   // 32-byte random hex
    "  user_id INTEGER NOT NULL,"
    "  expires_at INTEGER NOT NULL,"  // unix timestamp
    "  created_at INTEGER,"
    "  FOREIGN KEY(user_id) REFERENCES users(id)"
    ");"

    "CREATE INDEX IF NOT EXISTS idx_sessions_token ON sessions(token);"
    "CREATE INDEX IF NOT EXISTS idx_sessions_expiry ON sessions(expires_at);"

    // ============================================================
    // TAILSCALE MULTI-DEVICE CONFIG
    // ============================================================

    "CREATE TABLE IF NOT EXISTS device_config ("
    "  id INTEGER PRIMARY KEY,"
    "  device_name TEXT,"
    "  tailscale_ip TEXT,"
    "  role TEXT DEFAULT 'client',"  // server, client
    "  last_seen INTEGER,"
    "  is_active INTEGER DEFAULT 1"
    ");"

    // Index for performance
    "CREATE INDEX IF NOT EXISTS idx_journal_date ON journal_entries(entry_date);"
    "CREATE INDEX IF NOT EXISTS idx_journal_lines_journal ON journal_lines(journal_id);"
    "CREATE INDEX IF NOT EXISTS idx_journal_lines_account ON journal_lines(account_id);"
    "CREATE INDEX IF NOT EXISTS idx_invoices_customer ON invoices(customer_id);"
    "CREATE INDEX IF NOT EXISTS idx_invoices_status ON invoices(status);"
    "CREATE INDEX IF NOT EXISTS idx_bills_vendor ON bills(vendor_id);"
    "CREATE INDEX IF NOT EXISTS idx_contacts_type ON contacts(type);"
    "CREATE INDEX IF NOT EXISTS idx_deals_stage ON deals(stage);"
    "CREATE INDEX IF NOT EXISTS idx_bank_tx_matched ON bank_transactions(is_matched);"
    "CREATE INDEX IF NOT EXISTS idx_audit_entity ON audit_trail(entity_type, entity_id);";

static bool db_column_exists(sqlite3 *db, const char *table, const char *column) {
    sqlite3_stmt *stmt = NULL;
    char sql[128];
    snprintf(sql, sizeof(sql), "PRAGMA table_info(%s);", table);

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }

    bool found = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        if (name && strcmp(name, column) == 0) {
            found = true;
            break;
        }
    }

    sqlite3_finalize(stmt);
    return found;
}

static bool db_ensure_audit_details_column(sqlite3 *db) {
    if (db_column_exists(db, "audit_trail", "details")) {
        return true;
    }

    char *err = NULL;
    int rc = sqlite3_exec(db, "ALTER TABLE audit_trail ADD COLUMN details TEXT;", 0, 0, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Audit schema migration failed: %s\n", err ? err : sqlite3_errmsg(db));
        sqlite3_free(err);
        return false;
    }

    return true;
}

static void db_bytes_to_hex(const unsigned char *bytes, size_t len, char *hex_out) {
    for (size_t i = 0; i < len; i++) {
        sprintf(hex_out + (i * 2), "%02x", bytes[i]);
    }
    hex_out[len * 2] = '\0';
}

static bool db_has_users(sqlite3 *db) {
    sqlite3_stmt *stmt = NULL;
    bool has_users = false;

    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM users;", -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        has_users = sqlite3_column_int(stmt, 0) > 0;
    }

    sqlite3_finalize(stmt);
    return has_users;
}

static bool db_seed_bootstrap_admin(sqlite3 *db) {
    if (db_has_users(db)) {
        return true;
    }

    const char *bootstrap_username = getenv("QMANAGE_BOOTSTRAP_USERNAME");
    if (!bootstrap_username || bootstrap_username[0] == '\0') {
        bootstrap_username = "admin";
    }

    const char *bootstrap_password = getenv("QMANAGE_BOOTSTRAP_PASSWORD");
    char generated_password[25] = {0};
    if (!bootstrap_password || bootstrap_password[0] == '\0') {
        unsigned char random_password[12];
        mg_random(random_password, sizeof(random_password));
        db_bytes_to_hex(random_password, sizeof(random_password), generated_password);
        bootstrap_password = generated_password;
    }

    unsigned char salt_bytes[16];
    unsigned char key_bytes[32];
    char salt_hex[33];
    char hash_hex[65];
    mg_random(salt_bytes, sizeof(salt_bytes));
    db_bytes_to_hex(salt_bytes, sizeof(salt_bytes), salt_hex);
    crypto_derive_key(bootstrap_password, salt_bytes, 100000, key_bytes);
    crypto_hash_to_hex(key_bytes, hash_hex);

    sqlite3_stmt *stmt = NULL;
    const char *sql = "INSERT INTO users (id, username, password_hash, password_salt, role, created_at) "
                      "VALUES (1, ?, ?, ?, 'admin', strftime('%s','now'));";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare bootstrap admin insert: %s\n", sqlite3_errmsg(db));
        return false;
    }

    sqlite3_bind_text(stmt, 1, bootstrap_username, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, hash_hex, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, salt_hex, -1, SQLITE_TRANSIENT);

    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok) {
        fprintf(stderr, "Failed to seed bootstrap admin: %s\n", sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);

    if (!ok) {
        return false;
    }

    printf("Bootstrap admin ready. Username: %s\n", bootstrap_username);
    if (generated_password[0] != '\0') {
        printf("Generated bootstrap password: %s\n", generated_password);
        printf("Store this password now or set QMANAGE_BOOTSTRAP_PASSWORD before the first run.\n");
    } else {
        printf("Bootstrap password loaded from QMANAGE_BOOTSTRAP_PASSWORD.\n");
    }

    return true;
}

// ============================================================
// Database Initialization
// ============================================================
bool db_init(AppState *app) {
    int rc = sqlite3_open(DB_PATH, &app->db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(app->db));
        return false;
    }

    char *err = NULL;
    rc = sqlite3_exec(app->db, SCHEMA, 0, 0, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Schema Error: %s\n", err);
        sqlite3_free(err);
        return false;
    }

    if (!db_ensure_audit_details_column(app->db)) {
        return false;
    }

    // Seed default Chart of Accounts if empty
    sqlite3_exec(app->db,
        "INSERT OR IGNORE INTO accounts (id, name, code, type, is_system) VALUES "
        "(1,'Cash','1001',1,1),"
        "(2,'Bank','1002',1,1),"
        "(3,'Sales Revenue','4001',4,1),"
        "(4,'Accounts Receivable','1101',1,1),"
        "(5,'Accounts Payable','2101',2,1),"
        "(6,'Owner Equity','3001',3,1),"
        "(7,'Cost of Goods Sold','5001',5,1),"
        "(8,'Office Expenses','5002',5,1),"
        "(9,'Rent','5003',5,1),"
        "(10,'Utilities','5004',5,1),"
        "(11,'Interest Income','4002',4,1),"
        "(12,'Other Income','4003',4,1),"
        "(13,'GST Input Credit','1201',1,1),"
        "(14,'GST Output Liability','2201',2,1),"
        "(15,'TDS Payable','2301',2,1),"
        "(16,'Salary Expense','5005',5,1),"
        "(17,'PF Expense','5006',5,1),"
        "(18,'ESI Expense','5007',5,1),"
        "(19,'Advertising','5008',5,1),"
        "(20,'Travel','5009',5,1),"
        "(21,'Telephone & Internet','5010',5,1),"
        "(22,'Inventory Asset','1301',1,1),"
        "(23,'Retained Earnings','3002',3,1),"
        "(24,'Undeposited Funds','1102',1,1),"
        "(25,'Petty Cash','1003',1,1);",
        0, 0, 0);

    // Seed default organization if empty
    sqlite3_exec(app->db,
        "INSERT OR IGNORE INTO organizations (id, name, created_at) VALUES (1, 'My Organization', strftime('%s','now'));",
        0, 0, 0);

    // Seed default warehouse
    sqlite3_exec(app->db,
        "INSERT OR IGNORE INTO warehouses (id, name, is_primary, created_at) VALUES (1, 'Main Warehouse', 1, strftime('%s','now'));",
        0, 0, 0);

    if (!db_seed_bootstrap_admin(app->db)) {
        return false;
    }

    printf("Database initialized: 25 accounts, 1 warehouse configured.\n");
    return true;
}

// ============================================================
// Type-safe parameter binding helpers
// ============================================================

// Bind a DbParam array to a prepared statement (1-indexed slots)
static void db_bind_params(sqlite3_stmt *stmt, const DbParam *params, int count) {
    for (int i = 0; i < count; i++) {
        int slot = i + 1;
        switch (params[i].type) {
            case DB_INT:
                sqlite3_bind_int(stmt, slot, params[i].v.i);
                break;
            case DB_INT64:
                sqlite3_bind_int64(stmt, slot, params[i].v.i64);
                break;
            case DB_DOUBLE:
                sqlite3_bind_double(stmt, slot, params[i].v.d);
                break;
            case DB_TEXT:
                sqlite3_bind_text(stmt, slot, params[i].v.s ? params[i].v.s : "",
                                  -1, SQLITE_TRANSIENT);
                break;
            case DB_NULL:
                sqlite3_bind_null(stmt, slot);
                break;
        }
    }
}

// ============================================================
// Parameterized Query (SELECT) — returns cJSON array
// ============================================================
cJSON* db_query_params(AppState *app, const char *sql, int param_count, ...) {
    cJSON *results = cJSON_CreateArray();
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Query prepare error: %s\n", sqlite3_errmsg(app->db));
        return results;
    }

    // Legacy: va_list path binds everything as text (kept for raw query calls)
    if (param_count > 0) {
        va_list args;
        va_start(args, param_count);
        for (int i = 1; i <= param_count; i++) {
            void *val = va_arg(args, void*);
            sqlite3_bind_text(stmt, i, (const char*)val, -1, SQLITE_TRANSIENT);
        }
        va_end(args);
    }

    int cols = sqlite3_column_count(stmt);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON *row = cJSON_CreateObject();
        for (int i = 0; i < cols; i++) {
            const char *name = sqlite3_column_name(stmt, i);
            switch (sqlite3_column_type(stmt, i)) {
                case SQLITE_INTEGER:
                    cJSON_AddNumberToObject(row, name, sqlite3_column_int64(stmt, i));
                    break;
                case SQLITE_FLOAT:
                    cJSON_AddNumberToObject(row, name, sqlite3_column_double(stmt, i));
                    break;
                case SQLITE_TEXT:
                    cJSON_AddStringToObject(row, name, (const char*)sqlite3_column_text(stmt, i));
                    break;
                case SQLITE_NULL:
                    cJSON_AddNullToObject(row, name);
                    break;
            }
        }
        cJSON_AddItemToArray(results, row);
    }
    sqlite3_finalize(stmt);
    return results;
}

// Type-safe query using DbParam array
cJSON* db_query_typed(AppState *app, const char *sql, const DbParam *params, int count) {
    cJSON *results = cJSON_CreateArray();
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Query prepare error: %s\n", sqlite3_errmsg(app->db));
        return results;
    }

    db_bind_params(stmt, params, count);

    int cols = sqlite3_column_count(stmt);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON *row = cJSON_CreateObject();
        for (int i = 0; i < cols; i++) {
            const char *name = sqlite3_column_name(stmt, i);
            switch (sqlite3_column_type(stmt, i)) {
                case SQLITE_INTEGER:
                    cJSON_AddNumberToObject(row, name, sqlite3_column_int64(stmt, i));
                    break;
                case SQLITE_FLOAT:
                    cJSON_AddNumberToObject(row, name, sqlite3_column_double(stmt, i));
                    break;
                case SQLITE_TEXT:
                    cJSON_AddStringToObject(row, name, (const char*)sqlite3_column_text(stmt, i));
                    break;
                case SQLITE_NULL:
                    cJSON_AddNullToObject(row, name);
                    break;
            }
        }
        cJSON_AddItemToArray(results, row);
    }
    sqlite3_finalize(stmt);
    return results;
}

// ============================================================
// Parameterized Execute (INSERT/UPDATE/DELETE)
// ============================================================
bool db_execute_params(AppState *app, const char *sql, int param_count, ...) {
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Execute prepare error: %s\n", sqlite3_errmsg(app->db));
        return false;
    }

    // Legacy va_list path (text only — kept for backward compat)
    if (param_count > 0) {
        va_list args;
        va_start(args, param_count);
        for (int i = 1; i <= param_count; i++) {
            void *val = va_arg(args, void*);
            sqlite3_bind_text(stmt, i, (const char*)val, -1, SQLITE_TRANSIENT);
        }
        va_end(args);
    }

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok) fprintf(stderr, "Execute error: %s\n", sqlite3_errmsg(app->db));
    sqlite3_finalize(stmt);
    return ok;
}

// Type-safe execute using DbParam array
bool db_execute_typed(AppState *app, const char *sql, const DbParam *params, int count) {
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(app->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Execute prepare error: %s\n", sqlite3_errmsg(app->db));
        return false;
    }

    db_bind_params(stmt, params, count);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok) fprintf(stderr, "Execute error: %s\n", sqlite3_errmsg(app->db));
    sqlite3_finalize(stmt);
    return ok;
}

// ============================================================
// Raw Execute (for internal use — schema, transactions)
// ============================================================
bool db_execute(AppState *app, const char *sql) {
    char *err = NULL;
    int rc = sqlite3_exec(app->db, sql, 0, 0, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL Error: %s\n", err);
        sqlite3_free(err);
        return false;
    }
    return true;
}

// ============================================================
// Raw Query (for internal use — returns cJSON array)
// ============================================================
cJSON* db_query(AppState *app, const char *sql) {
    return db_query_params(app, sql, 0);
}

// ============================================================
// Cleanup
// ============================================================
void db_close(AppState *app) {
    if (app->db) {
        sqlite3_close(app->db);
        app->db = NULL;
    }
}
