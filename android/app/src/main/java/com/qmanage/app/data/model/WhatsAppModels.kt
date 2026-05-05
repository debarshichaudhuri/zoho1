package com.qmanage.app.data.model

// WhatsApp Business API Models

data class WhatsAppConfig(
    val apiKey: String = "",
    val phoneNumberId: String = "",
    val businessAccountId: String = "",
    val isEnabled: Boolean = false
)

data class WhatsAppMessageRequest(
    val to: String,  // Phone number in international format
    val type: String = "text",
    val text: WhatsAppText? = null,
    val template: WhatsAppTemplate? = null
)

data class WhatsAppText(
    val body: String,
    val previewUrl: Boolean = false
)

data class WhatsAppTemplate(
    val name: String,
    val language: WhatsAppLanguage,
    val components: List<TemplateComponent>? = null
)

data class WhatsAppLanguage(
    val code: String,  // e.g., "en", "hi"
    val policy: String = "deterministic"
)

data class TemplateComponent(
    val type: String,  // "header", "body", "button"
    val parameters: List<TemplateParameter>
)

data class TemplateParameter(
    val type: String,  // "text", "currency", "date_time"
    val text: String? = null,
    val currency: CurrencyParameter? = null
)

data class CurrencyParameter(
    val fallbackValue: String,
    val code: String,  // "INR"
    val amount1000: Long  // Amount in smallest currency unit
)

data class WhatsAppMessageResponse(
    val messagingProduct: String = "whatsapp",
    val contacts: List<ContactResponse>? = null,
    val messages: List<MessageResponse>? = null,
    val error: WhatsAppError? = null
)

data class ContactResponse(
    val input: String,
    val waId: String
)

data class MessageResponse(
    val id: String,
    val messageStatus: String = "accepted"
)

data class WhatsAppError(
    val code: Int,
    val message: String,
    val type: String
)

// Reminder Types
data class ReminderRequest(
    val recipientPhone: String,
    val reminderType: ReminderType,
    val amount: Double? = null,
    val dueDate: String? = null,
    val contactName: String = "",
    val notes: String = ""
)

enum class ReminderType {
    PAYMENT_DUE,
    INVOICE_OVERDUE,
    QUOTE_FOLLOWUP,
    CUSTOM_MESSAGE
}

// Anti-hallucination verification data
data class VerifiedQueryResult(
    val query: String,
    val rawResult: String,
    val isVerified: Boolean,
    val verificationMethod: VerificationMethod,
    val dataSource: String,
    val confidenceScore: Double,  // 0.0 to 1.0
    val z3Constraints: List<String>? = null
)

enum class VerificationMethod {
    DATABASE_QUERY,      // Direct SQL query result
    Z3_SMT_SOLVER,       // Formal verification with Z3
    RULE_BASED_CHECK,    // Business rule validation
    KNOWLEDGE_BASE,      // Verified documentation
    UNVERIFIED           // Cannot verify - may be hallucinated
}

// Financial verification constraints for Z3
data class FinancialConstraint(
    val type: ConstraintType,
    val description: String,
    val expression: String  // Z3 SMT-LIB expression or plain English for simple rules
)

enum class ConstraintType {
    BALANCE_CHECK,       // Assets = Liabilities + Equity
    POSITIVE_AMOUNT,     // Amount must be > 0
    DATE_RANGE,          // Date must be within valid range
    ACCOUNT_EXISTS,      // Account ID must exist
    CURRENCY_MATCH,      // Currency must match transaction context
    PERMISSION_CHECK     // User has permission for this operation
}
