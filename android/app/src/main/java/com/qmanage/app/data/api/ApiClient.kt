package com.qmanage.app.data.api

import okhttp3.Interceptor
import okhttp3.OkHttpClient
import okhttp3.logging.HttpLoggingInterceptor
import retrofit2.Retrofit
import retrofit2.converter.gson.GsonConverterFactory
import java.util.concurrent.TimeUnit

object ApiClient {
    private var baseUrl: String = "http://localhost:9741/api/"
    private var authToken: String = ""

    private val authInterceptor = Interceptor { chain ->
        val req = chain.request().newBuilder()
            .apply { if (authToken.isNotEmpty()) header("X-Auth-Token", authToken) }
            .build()
        chain.proceed(req)
    }

    private val logging = HttpLoggingInterceptor().apply {
        level = HttpLoggingInterceptor.Level.BODY
    }

    private fun buildClient(): OkHttpClient = OkHttpClient.Builder()
        .addInterceptor(authInterceptor)
        .addInterceptor(logging)
        .connectTimeout(10, TimeUnit.SECONDS)
        .readTimeout(30, TimeUnit.SECONDS)
        .build()

    private var _api: QManageApi? = null

    val api: QManageApi
        get() = _api ?: buildApi().also { _api = it }

    private fun buildApi(): QManageApi = Retrofit.Builder()
        .baseUrl(baseUrl)
        .client(buildClient())
        .addConverterFactory(GsonConverterFactory.create())
        .build()
        .create(QManageApi::class.java)

    fun configure(serverUrl: String, token: String = "") {
        // Validate URL - must not be empty and must have scheme
        val validatedUrl = when {
            serverUrl.isBlank() -> baseUrl
            !serverUrl.startsWith("http://") && !serverUrl.startsWith("https://") -> baseUrl
            else -> serverUrl
        }
        val url = if (validatedUrl.endsWith("/")) validatedUrl else "$validatedUrl/"
        if (url != baseUrl || token != authToken) {
            baseUrl = url
            authToken = token
            _api = null // force rebuild
        }
    }

    fun setToken(token: String) {
        if (token != authToken) {
            authToken = token
            _api = null
        }
    }

    fun clearToken() {
        authToken = ""
        _api = null
    }

    fun isAuthenticated() = authToken.isNotEmpty()
}
