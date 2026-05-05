#ifndef BACKUP_H
#define BACKUP_H

#include "qmanage.h"

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

// Core Backup Operations
char* backup_create(AppState *app, const char *backup_dir, const char *passphrase);
bool backup_restore(AppState *app, const char *filepath, const char *passphrase);
cJSON* backup_list(AppState *app);

// API Route Handlers
void route_backup_create(struct mg_connection *c, struct mg_http_message *hm, AppState *app);
void route_backup_list(struct mg_connection *c, struct mg_http_message *hm, AppState *app);

#endif // BACKUP_H
