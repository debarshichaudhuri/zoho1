#ifndef QMANAGE_H
#define QMANAGE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <sys/stat.h>

#ifndef MKDIR_DEFINED
#define MKDIR_DEFINED
#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#define fseeko fseek
#define ftello ftell
#endif
#endif

// Dependencies
#include "mongoose.h"
#include "sqlite3.h"
#include "cJSON.h"

// ============================================================
// Common Utilities
// ============================================================
// Prototyped later after AppState

// ============================================================
// Configuration
// ============================================================
#define PORT            "9741"
#define DB_PATH         "data/qmanage.db"
#define BACKUP_DIR      "backups"
#define APP_VERSION     "2.1.0-SECURE"
#define MAX_THREADS     8

// ============================================================
// Financial Status Codes
// ============================================================
typedef enum {
    FIN_OK = 0,
    FIN_ERR_BALANCE  = 101,   // Debits != Credits
    FIN_ERR_DB       = 102,   // Database error
    FIN_ERR_NOT_FOUND = 103,  // Record not found
    FIN_ERR_AUTH     = 104,   // Authorization error
    FIN_ERR_LOCKED   = 105    // Period locked
} FinResult;

// ============================================================
// Account Types (Indian Accounting Standard)
// ============================================================
typedef enum {
    ACC_ASSET     = 1,
    ACC_LIABILITY = 2,
    ACC_EQUITY    = 3,
    ACC_INCOME    = 4,
    ACC_EXPENSE   = 5
} AccountType;

// ============================================================
// App State
// ============================================================
typedef struct {
    sqlite3 *db;
    struct mg_mgr mgr;
    bool is_running;
} AppState;

// ============================================================
// Type-safe DB parameter system
// ============================================================
typedef enum { DB_TEXT, DB_INT, DB_INT64, DB_DOUBLE, DB_NULL } DbParamType;

typedef struct {
    DbParamType type;
    union {
        const char *s;
        int         i;
        int64_t     i64;
        double      d;
    } v;
} DbParam;

// Convenience macros for building DbParam arrays
#define DB_P_TEXT(x)   { DB_TEXT,  { .s   = (x) } }
#define DB_P_INT(x)    { DB_INT,   { .i   = (x) } }
#define DB_P_INT64(x)  { DB_INT64, { .i64 = (x) } }
#define DB_P_DOUBLE(x) { DB_DOUBLE,{ .d   = (x) } }
#define DB_P_NULL      { DB_NULL,  { .i   = 0   } }

// ============================================================
// Database Core (db.c)
// ============================================================
bool   db_init(AppState *app);
void   db_close(AppState *app);
bool   db_execute(AppState *app, const char *sql);
cJSON* db_query(AppState *app, const char *sql);
cJSON* db_query_params(AppState *app, const char *sql, int param_count, ...);
bool   db_execute_params(AppState *app, const char *sql, int param_count, ...);
// Type-safe variants (prefer these over the variadic versions)
cJSON* db_query_typed(AppState *app, const char *sql, const DbParam *params, int count);
bool   db_execute_typed(AppState *app, const char *sql, const DbParam *params, int count);

// ============================================================
// Accounting Ledger Engine (ledger.c)
// ============================================================
FinResult ledger_record_entry(AppState *app, const char *memo, int64_t timestamp, cJSON *lines);
double    ledger_get_account_balance(AppState *app, int64_t account_id, int64_t start_date, int64_t end_date);
int64_t   ledger_get_account_balance_paisa(AppState *app, int64_t account_id, int64_t start_date, int64_t end_date);

// ============================================================
// Authentication (auth.c)
// ============================================================
void route_auth_login(struct mg_connection *c, struct mg_http_message *hm, AppState *app);
void route_auth_logout(struct mg_connection *c, struct mg_http_message *hm, AppState *app);
void route_auth_change_password(struct mg_connection *c, struct mg_http_message *hm,
                                AppState *app, int user_id);
// Returns user_id on success, 0 + sends 401 on failure
int  auth_validate(struct mg_connection *c, struct mg_http_message *hm, AppState *app);

// ============================================================
// API Layer (api.c)
// ============================================================
void handle_api_request(struct mg_connection *c, struct mg_http_message *hm, AppState *app);

// Route Handlers
void route_dashboard(struct mg_connection *c, struct mg_http_message *hm, AppState *app);
void route_contacts(struct mg_connection *c, struct mg_http_message *hm, AppState *app);
void route_items(struct mg_connection *c, struct mg_http_message *hm, AppState *app);
void route_invoices(struct mg_connection *c, struct mg_http_message *hm, AppState *app);
void route_ledger(struct mg_connection *c, struct mg_http_message *hm, AppState *app);
void route_accounts(struct mg_connection *c, struct mg_http_message *hm, AppState *app);

void send_json(struct mg_connection *c, int status, cJSON *data);
void send_error(struct mg_connection *c, int status, const char *msg);
void get_pagination(struct mg_http_message *hm, int *limit, int *offset);
int  mg_vcasecmp(const struct mg_str *s1, const char *s2);
int  audit_verify_chain(AppState *app);

#endif // QMANAGE_H

