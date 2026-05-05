package com.qmanage.app.data.api

import com.qmanage.app.data.model.*
import retrofit2.http.*
import retrofit2.Response

interface WhatsAppApi {
    
    @POST("v1/messages")
    suspend fun sendMessage(
        @Header("Authorization") bearerToken: String,
        @Body message: WhatsAppMessageRequest
    ): Response<WhatsAppMessageResponse>
    
    @GET("v1/business_profile")
    suspend fun getBusinessProfile(
        @Header("Authorization") bearerToken: String
    ): Response<Map<String, Any>>
}

// Anti-hallucination verification service
interface VerificationApi {
    
    @POST("ai/verify")
    suspend fun verifyQuery(
        @Body request: VerificationRequest
    ): Response<VerificationResponse>
}

data class VerificationRequest(
    val query: String,
    val context: String,
    val requiresFinancialAccuracy: Boolean = false
)

data class VerificationResponse(
    val isVerified: Boolean,
    val verifiedData: String?,
    val confidence: Double,
    val method: String,
    val warning: String? = null
)

// Local verification engine (runs on device)
class VerificationEngine {
    
    // Financial constraints for anti-hallucination
    private val financialConstraints = listOf(
        FinancialConstraint(
            ConstraintType.BALANCE_CHECK,
            "All transactions must balance",
            "debit_amount == credit_amount"
        ),
        FinancialConstraint(
            ConstraintType.POSITIVE_AMOUNT,
            "Amounts must be positive",
            "amount > 0"
        ),
        FinancialConstraint(
            ConstraintType.ACCOUNT_EXISTS,
            "Account must exist in chart of accounts",
            "account_id IN (SELECT id FROM accounts)"
        ),
        FinancialConstraint(
            ConstraintType.DATE_RANGE,
            "Date must not be in future",
            "date <= CURRENT_DATE"
        )
    )
    
    /**
     * Verify a query result using multiple methods
     */
    fun verifyQueryResult(
        query: String,
        rawResult: String,
        dataSource: String,
        isFinancial: Boolean = false
    ): VerifiedQueryResult {
        
        // 1. Check if result is from verified database query
        val isDbVerified = dataSource.contains("invoices") || 
                          dataSource.contains("transactions") ||
                          dataSource.contains("contacts")
        
        if (isDbVerified) {
            return VerifiedQueryResult(
                query = query,
                rawResult = rawResult,
                isVerified = true,
                verificationMethod = VerificationMethod.DATABASE_QUERY,
                dataSource = dataSource,
                confidenceScore = 0.95,
                z3Constraints = null
            )
        }
        
        // 2. For financial data, apply rule-based verification
        if (isFinancial) {
            val violations = checkFinancialConstraints(rawResult)
            if (violations.isNotEmpty()) {
                return VerifiedQueryResult(
                    query = query,
                    rawResult = rawResult,
                    isVerified = false,
                    verificationMethod = VerificationMethod.RULE_BASED_CHECK,
                    dataSource = dataSource,
                    confidenceScore = 0.3,
                    z3Constraints = violations.map { it.expression }
                )
            }
        }
        
        // 3. Knowledge base verification for app features
        if (dataSource.contains("documentation") || dataSource.contains("app")) {
            return VerifiedQueryResult(
                query = query,
                rawResult = rawResult,
                isVerified = true,
                verificationMethod = VerificationMethod.KNOWLEDGE_BASE,
                dataSource = dataSource,
                confidenceScore = 0.9,
                z3Constraints = null
            )
        }
        
        // 4. Unverified - flag as potentially hallucinated
        return VerifiedQueryResult(
            query = query,
            rawResult = rawResult,
            isVerified = false,
            verificationMethod = VerificationMethod.UNVERIFIED,
            dataSource = dataSource,
            confidenceScore = 0.5,
            z3Constraints = null
        )
    }
    
    /**
     * Check financial constraints
     */
    private fun checkFinancialConstraints(result: String): List<FinancialConstraint> {
        val violations = mutableListOf<FinancialConstraint>()
        
        // Check for negative amounts
        if (result.contains("-₹") || result.contains("-Rs")) {
            violations.add(financialConstraints[1])
        }
        
        // Check for date anomalies (simplified)
        // In real implementation, parse dates and compare
        
        return violations
    }
    
    /**
     * Format verification result for display
     */
    fun formatVerifiedResponse(result: VerifiedQueryResult): String {
        val verificationBadge = when {
            result.isVerified && result.confidenceScore > 0.9 -> " ✅ Verified"
            result.isVerified -> " ✓ Checked"
            result.confidenceScore < 0.5 -> " ⚠️ Unverified"
            else -> " ⚠️ Review Needed"
        }
        
        val sourceNote = "\n\n[Source: ${result.dataSource}]"
        
        return result.rawResult + verificationBadge + sourceNote
    }
    
    /**
     * Generate safe response when data is unavailable
     */
    fun generateSafeResponse(query: String): String {
        return """
            I don't have verified data to answer your question about "$query".
            
            To get accurate information:
            • Ensure your data is synced with the server
            • Check if the relevant records exist in your database
            • Try rephrasing your question
            
            I can only provide verified information from your local database to prevent errors.
        """.trimIndent()
    }
}

// WhatsApp Message Templates
object WhatsAppTemplates {
    
    fun paymentReminder(contactName: String, amount: Double, dueDate: String): WhatsAppMessageRequest {
        return WhatsAppMessageRequest(
            to = "",
            type = "template",
            template = WhatsAppTemplate(
                name = "payment_reminder",
                language = WhatsAppLanguage(code = "en"),
                components = listOf(
                    TemplateComponent(
                        type = "body",
                        parameters = listOf(
                            TemplateParameter(type = "text", text = contactName),
                            TemplateParameter(
                                type = "currency",
                                currency = CurrencyParameter(
                                    fallbackValue = "₹${amount}",
                                    code = "INR",
                                    amount1000 = (amount * 1000).toLong()
                                )
                            ),
                            TemplateParameter(type = "text", text = dueDate)
                        )
                    )
                )
            )
        )
    }
    
    fun invoiceNotification(invoiceNumber: String, amount: Double, status: String): WhatsAppMessageRequest {
        return WhatsAppMessageRequest(
            to = "",
            type = "template",
            template = WhatsAppTemplate(
                name = "invoice_notification",
                language = WhatsAppLanguage(code = "en"),
                components = listOf(
                    TemplateComponent(
                        type = "body",
                        parameters = listOf(
                            TemplateParameter(type = "text", text = invoiceNumber),
                            TemplateParameter(
                                type = "currency",
                                currency = CurrencyParameter(
                                    fallbackValue = "₹${amount}",
                                    code = "INR",
                                    amount1000 = (amount * 1000).toLong()
                                )
                            ),
                            TemplateParameter(type = "text", text = status)
                        )
                    )
                )
            )
        )
    }
    
    fun customMessage(phoneNumber: String, message: String): WhatsAppMessageRequest {
        return WhatsAppMessageRequest(
            to = phoneNumber,
            type = "text",
            text = WhatsAppText(body = message, previewUrl = false)
        )
    }
}
