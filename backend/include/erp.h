#ifndef ERP_H
#define ERP_H

#include "qmanage.h"

// ============================================================
// ERP Module: Bills, Payments, Quotes, CRM Deals, Inventory
// Replaces Zoho Books + CRM + Analytics in one unified header
// ============================================================

// --- Bills (Vendor Invoices / Accounts Payable) ---
void route_bills(struct mg_connection *c, struct mg_http_message *hm, AppState *app);

// --- Payments Received (Against Invoices) ---
void route_payments_received(struct mg_connection *c, struct mg_http_message *hm, AppState *app);

// --- Payments Made (Against Bills) ---
void route_payments_made(struct mg_connection *c, struct mg_http_message *hm, AppState *app);

// --- Quotes / Estimates ---
void route_quotes(struct mg_connection *c, struct mg_http_message *hm, AppState *app);
void route_quote_convert(struct mg_connection *c, struct mg_http_message *hm, AppState *app);

// --- CRM Deals Pipeline ---
void route_deals(struct mg_connection *c, struct mg_http_message *hm, AppState *app);
void route_deal_update(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id);
void route_deal_delete(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id);

// --- Inventory Movements ---
void route_inventory(struct mg_connection *c, struct mg_http_message *hm, AppState *app);

// --- Credit Notes (Sales Returns) ---
void route_credit_notes(struct mg_connection *c, struct mg_http_message *hm, AppState *app);
void route_credit_note_update(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id);
void route_credit_note_delete(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id);

// --- Vendor Credits (Debit Notes) ---
void route_vendor_credits(struct mg_connection *c, struct mg_http_message *hm, AppState *app);
void route_vendor_credit_update(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id);
void route_vendor_credit_delete(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id);

// --- Purchase Orders ---
void route_purchase_orders(struct mg_connection *c, struct mg_http_message *hm, AppState *app);
void route_purchase_order_update(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id);
void route_purchase_order_delete(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id);

// --- Contact CRUD ---
void route_contact_update(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id);

// --- Invoice CRUD ---
void route_invoice_void(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id);
void route_invoice_update(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id);

// --- Bill CRUD ---
void route_bill_update(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id);
void route_bill_delete(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id);

// --- Item CRUD ---
void route_item_update(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id);
void route_item_delete(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id);

// --- Quote CRUD ---
void route_quote_update(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id);
void route_quote_delete(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id);

// --- Reports Engine ---
void route_report_balance_sheet(struct mg_connection *c, struct mg_http_message *hm, AppState *app);
void route_report_trial_balance(struct mg_connection *c, struct mg_http_message *hm, AppState *app);
void route_report_cash_flow(struct mg_connection *c, struct mg_http_message *hm, AppState *app);
void route_report_aging(struct mg_connection *c, struct mg_http_message *hm, AppState *app);
void route_report_sales_by_customer(struct mg_connection *c, struct mg_http_message *hm, AppState *app);
void route_report_expense_by_category(struct mg_connection *c, struct mg_http_message *hm, AppState *app);
void route_report_gst_summary(struct mg_connection *c, struct mg_http_message *hm, AppState *app);

// --- Global Search ---
void route_search(struct mg_connection *c, struct mg_http_message *hm, AppState *app);

// --- GST Compliance (e-Invoice / e-Way Bill) ---
void route_gst_einvoice(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int invoice_id);
void route_gst_eway(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int invoice_id);
void route_gst_summary(struct mg_connection *c, struct mg_http_message *hm, AppState *app);

// --- Gmail IMAP Bot ---
void route_gmail_config(struct mg_connection *c, struct mg_http_message *hm, AppState *app);
void route_gmail_fetch(struct mg_connection *c, struct mg_http_message *hm, AppState *app);
void route_gmail_status(struct mg_connection *c, struct mg_http_message *hm, AppState *app);
void route_email_send(struct mg_connection *c, struct mg_http_message *hm, AppState *app);

// --- Google Drive Backup ---
void route_gdrive_config(struct mg_connection *c, struct mg_http_message *hm, AppState *app);
void route_gdrive_upload(struct mg_connection *c, struct mg_http_message *hm, AppState *app);
void route_gdrive_list(struct mg_connection *c, struct mg_http_message *hm, AppState *app);

// --- Activities (CRM Follow-ups & Task Log) ---
void route_activities(struct mg_connection *c, struct mg_http_message *hm, AppState *app);
void route_activity_update(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id);

// --- Leads (CRM Pre-deal Prospects) ---
void route_leads(struct mg_connection *c, struct mg_http_message *hm, AppState *app);
void route_lead_update(struct mg_connection *c, struct mg_http_message *hm, AppState *app, int id);

// Utilities
void send_json(struct mg_connection *c, int status, cJSON *data);
void send_error(struct mg_connection *c, int status, const char *error_message);
void get_pagination(struct mg_http_message *hm, int *limit, int *offset);
int mg_vcasecmp(const struct mg_str *s1, const char *s2);

#endif // ERP_H
