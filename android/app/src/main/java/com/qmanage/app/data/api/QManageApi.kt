package com.qmanage.app.data.api

import com.qmanage.app.data.model.*
import retrofit2.http.*

interface QManageApi {

    // Auth
    @POST("auth/login")
    suspend fun login(@Body request: LoginRequest): LoginResponse

    @POST("auth/logout")
    suspend fun logout(): ApiResponse

    @POST("auth/change-password")
    suspend fun changePassword(@Body body: Map<String, String>): ApiResponse

    // Dashboard
    @GET("dashboard")
    suspend fun getDashboard(): DashboardResponse

    // Contacts
    @GET("contacts")
    suspend fun getContacts(
        @Query("limit") limit: Int = 50,
        @Query("offset") offset: Int = 0
    ): List<Contact>

    @POST("contacts")
    suspend fun createContact(@Body contact: CreateContactRequest): ApiResponse

    @PUT("contacts/{id}")
    suspend fun updateContact(@Path("id") id: Long, @Body fields: Map<String, String>): ApiResponse

    @DELETE("contacts/{id}")
    suspend fun deleteContact(@Path("id") id: Long): ApiResponse

    // Invoices
    @GET("invoices")
    suspend fun getInvoices(
        @Query("limit") limit: Int = 50,
        @Query("offset") offset: Int = 0
    ): List<Invoice>

    @POST("invoices")
    suspend fun createInvoice(@Body invoice: CreateInvoiceRequest): ApiResponse

    @PUT("invoices/{id}")
    suspend fun updateInvoice(@Path("id") id: Long, @Body fields: Map<String, Any>): ApiResponse

    @POST("invoices/{id}/void")
    suspend fun voidInvoice(@Path("id") id: Long): ApiResponse

    // Bills
    @GET("bills")
    suspend fun getBills(
        @Query("limit") limit: Int = 50,
        @Query("offset") offset: Int = 0
    ): List<Bill>

    // Expenses
    @GET("expenses")
    suspend fun getExpenses(
        @Query("limit") limit: Int = 50,
        @Query("offset") offset: Int = 0
    ): List<Expense>

    @POST("expenses")
    suspend fun createExpense(@Body expense: CreateExpenseRequest): ApiResponse

    // Items
    @GET("items")
    suspend fun getItems(
        @Query("limit") limit: Int = 50,
        @Query("offset") offset: Int = 0
    ): List<Item>

    // Quotes
    @GET("quotes")
    suspend fun getQuotes(
        @Query("limit") limit: Int = 50,
        @Query("offset") offset: Int = 0
    ): List<Quote>

    @POST("quotes")
    suspend fun createQuote(@Body quote: CreateQuoteRequest): ApiResponse

    // Deals
    @GET("deals")
    suspend fun getDeals(): DealsResponse

    @POST("deals")
    suspend fun createDeal(@Body deal: CreateDealRequest): ApiResponse
}
