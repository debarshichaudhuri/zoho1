package com.qmanage.app.data.model

import com.google.gson.annotations.SerializedName

// ============================================================
// Auth
// ============================================================
data class LoginRequest(val username: String, val password: String)

data class LoginResponse(
    val token: String,
    @SerializedName("expires_in") val expiresIn: Long,
    val role: String
)

// ============================================================
// Dashboard
// ============================================================
data class DashboardCounts(
    @SerializedName("total_contacts")       val totalContacts: Long = 0,
    @SerializedName("total_invoices")       val totalInvoices: Long = 0,
    @SerializedName("unpaid_invoices")      val unpaidInvoices: Long = 0,
    @SerializedName("total_bills")          val totalBills: Long = 0,
    @SerializedName("unpaid_bills")         val unpaidBills: Long = 0,
    @SerializedName("active_deals")         val activeDeals: Long = 0,
    @SerializedName("total_receivable")     val totalReceivable: Long = 0,
    @SerializedName("total_payable")        val totalPayable: Long = 0,
    @SerializedName("low_stock_items")      val lowStockItems: Long = 0,
    @SerializedName("unmatched_transactions") val unmatchedTx: Long = 0
)

data class RecentInvoice(
    @SerializedName("invoice_num") val invoiceNum: String = "",
    val customer: String = "",
    val total: Long = 0,
    val status: String = "",
    val date: Long = 0
)

data class DashboardResponse(
    @SerializedName("total_assets")      val totalAssets: Double = 0.0,
    @SerializedName("total_income")      val totalIncome: Double = 0.0,
    @SerializedName("total_expenses")    val totalExpenses: Double = 0.0,
    @SerializedName("total_liabilities") val totalLiabilities: Double = 0.0,
    @SerializedName("net_profit")        val netProfit: Double = 0.0,
    val counts: DashboardCounts = DashboardCounts(),
    @SerializedName("recent_invoices")   val recentInvoices: List<RecentInvoice> = emptyList()
)

// ============================================================
// Contacts
// ============================================================
data class Contact(
    val id: Long = 0,
    val type: String = "customer",
    @SerializedName("display_name") val displayName: String = "",
    @SerializedName("company_name") val companyName: String = "",
    val email: String = "",
    val phone: String = "",
    val gstin: String = "",
    @SerializedName("billing_city") val billingCity: String = "",
    @SerializedName("billing_state") val billingState: String = "",
    @SerializedName("is_active") val isActive: Int = 1
)

data class CreateContactRequest(
    @SerializedName("display_name") val displayName: String,
    val type: String,
    val email: String = "",
    val phone: String = "",
    val gstin: String = "",
    @SerializedName("company_name") val companyName: String = ""
)

// ============================================================
// Invoices
// ============================================================
data class Invoice(
    val id: Long = 0,
    @SerializedName("invoice_num") val invoiceNum: String = "",
    @SerializedName("customer_name") val customerName: String = "",
    @SerializedName("customer_id") val customerId: Long = 0,
    val total: Long = 0,
    @SerializedName("balance_due") val balanceDue: Long = 0,
    @SerializedName("amount_paid") val amountPaid: Long = 0,
    val status: String = "draft",
    val date: Long = 0,
    @SerializedName("due_date") val dueDate: Long = 0,
    val notes: String = ""
)

data class CreateInvoiceRequest(
    @SerializedName("invoice_num") val invoiceNum: String,
    @SerializedName("customer_id") val customerId: Long,
    val total: Long,          // paisa
    val subtotal: Long,       // paisa
    val tax: Long = 0,        // paisa
    val date: Long,
    val notes: String = ""
)

// ============================================================
// Expenses
// ============================================================
data class Expense(
    val id: Long = 0,
    @SerializedName("entry_date") val entryDate: Long = 0,
    val memo: String = "",
    val amount: Long = 0,
    val category: String = ""
)

data class CreateExpenseRequest(
    val category: String,
    val amount: Long,   // paisa
    val date: Long,
    val notes: String = ""
)

// ============================================================
// Bills
// ============================================================
data class Bill(
    val id: Long = 0,
    @SerializedName("bill_num") val billNum: String = "",
    @SerializedName("vendor_name") val vendorName: String = "",
    @SerializedName("vendor_id") val vendorId: Long = 0,
    val total: Long = 0,
    @SerializedName("balance_due") val balanceDue: Long = 0,
    val status: String = "draft",
    val date: Long = 0
)

// ============================================================
// Items
// ============================================================
data class Item(
    val id: Long = 0,
    val name: String = "",
    val sku: String = "",
    val price: Long = 0,
    @SerializedName("cost_price") val costPrice: Long = 0,
    @SerializedName("tax_rate") val taxRate: Double = 18.0,
    val stock: Int = 0,
    val unit: String = "pcs",
    @SerializedName("hsn_code") val hsnCode: String = ""
)

// ============================================================
// Quotes / Estimates
// ============================================================
data class Quote(
    val id: Long = 0,
    @SerializedName("quote_num") val quoteNum: String = "",
    @SerializedName("customer_name") val customerName: String = "",
    @SerializedName("customer_id") val customerId: Long = 0,
    val total: Long = 0,
    val status: String = "draft",
    val date: Long = 0,
    @SerializedName("expiry_date") val expiryDate: Long = 0,
    val notes: String = ""
)

data class CreateQuoteRequest(
    @SerializedName("quote_num") val quoteNum: String,
    @SerializedName("customer_id") val customerId: Long,
    val total: Long,
    val subtotal: Long,
    val date: Long,
    val notes: String = ""
)

// ============================================================
// CRM Deals
// ============================================================
data class Deal(
    val id: Long = 0,
    val title: String = "",
    @SerializedName("contact_id") val contactId: Long = 0,
    @SerializedName("contact_name") val contactName: String = "",
    val stage: String = "contacted",
    val amount: Long = 0,
    val probability: Int = 50,
    @SerializedName("expected_close") val expectedClose: Long = 0,
    val source: String = "",
    val notes: String = "",
    @SerializedName("created_at") val createdAt: Long = 0
)

data class CreateDealRequest(
    val title: String,
    @SerializedName("contact_id") val contactId: Long = 0,
    val stage: String = "contacted",
    val amount: Long = 0,
    val probability: Int = 50,
    val notes: String = "",
    val source: String = "manual",
    @SerializedName("expected_close") val expectedClose: String? = null
)

data class PipelineStage(
    val stage: String = "",
    val count: Long = 0,
    val total: Long = 0
)

data class DealsResponse(
    val deals: List<Deal> = emptyList(),
    val pipeline: List<PipelineStage> = emptyList()
)

// ============================================================
// Generic API response
// ============================================================
data class ApiResponse(val status: String = "", val error: String = "", val id: Long = 0)
