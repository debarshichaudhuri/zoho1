#ifndef MIGRATION_H
#define MIGRATION_H

#include "qmanage.h"

// Migration Wizard API
void route_migration_import(struct mg_connection *c, struct mg_http_message *hm, AppState *app);

// CSV Processors
int process_zoho_invoices_csv(AppState *app, const char *csv_data);
int process_zoho_contacts_csv(AppState *app, const char *csv_data);

#endif // MIGRATION_H
