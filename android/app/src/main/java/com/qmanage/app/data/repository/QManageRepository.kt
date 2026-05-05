package com.qmanage.app.data.repository

import com.qmanage.app.data.api.ApiClient
import com.qmanage.app.data.model.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

sealed class Result<out T> {
    data class Success<T>(val data: T) : Result<T>()
    data class Error(val message: String) : Result<Nothing>()
}

class QManageRepository {
    private val api get() = ApiClient.api

    private suspend fun <T> safeCall(block: suspend () -> T): Result<T> =
        withContext(Dispatchers.IO) {
            try { Result.Success(block()) }
            catch (e: Exception) { Result.Error(e.message ?: "Unknown error") }
        }

    // Auth
    suspend fun login(username: String, password: String) =
        safeCall { api.login(LoginRequest(username, password)) }

    suspend fun logout() = safeCall { api.logout() }

    // Dashboard
    suspend fun getDashboard() = safeCall { api.getDashboard() }

    // Contacts
    suspend fun getContacts(limit: Int = 50, offset: Int = 0) =
        safeCall { api.getContacts(limit, offset) }

    suspend fun createContact(req: CreateContactRequest) =
        safeCall { api.createContact(req) }

    suspend fun deleteContact(id: Long) = safeCall { api.deleteContact(id) }

    // Invoices
    suspend fun getInvoices(limit: Int = 50, offset: Int = 0) =
        safeCall { api.getInvoices(limit, offset) }

    suspend fun createInvoice(req: CreateInvoiceRequest) =
        safeCall { api.createInvoice(req) }

    suspend fun voidInvoice(id: Long) = safeCall { api.voidInvoice(id) }

    // Bills
    suspend fun getBills(limit: Int = 50, offset: Int = 0) =
        safeCall { api.getBills(limit, offset) }

    // Expenses
    suspend fun getExpenses(limit: Int = 50, offset: Int = 0) =
        safeCall { api.getExpenses(limit, offset) }

    suspend fun createExpense(req: CreateExpenseRequest) =
        safeCall { api.createExpense(req) }

    // Items
    suspend fun getItems(limit: Int = 50, offset: Int = 0) =
        safeCall { api.getItems(limit, offset) }

    // Quotes
    suspend fun getQuotes(limit: Int = 50, offset: Int = 0) =
        safeCall { api.getQuotes(limit, offset) }

    suspend fun createQuote(req: CreateQuoteRequest) =
        safeCall { api.createQuote(req) }

    // Deals
    suspend fun getDeals() = safeCall { api.getDeals() }

    suspend fun createDeal(req: CreateDealRequest) =
        safeCall { api.createDeal(req) }
}
