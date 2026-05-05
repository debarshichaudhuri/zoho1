#ifndef BANKING_H
#define BANKING_H

#include "qmanage.h"

typedef enum {
    TX_CREDIT,
    TX_DEBIT
} TransactionType;

typedef struct {
    char date[32];
    double amount;
    TransactionType type;
    char account_ref[32];
    char utr[64];
    char description[256];
} BankTransaction;

// Core Banking Bridge API
void route_banking_sms(struct mg_connection *c, struct mg_http_message *hm, AppState *app);
void route_banking_reconcile(struct mg_connection *c, struct mg_http_message *hm, AppState *app);
void route_banking_statement(struct mg_connection *c, struct mg_http_message *hm, AppState *app);

// Parsers
bool parse_bank_sms(const char *sms_text, BankTransaction *tx);

// Auto Reconciliation
void reconcile_transactions(AppState *app);

#endif // BANKING_H
