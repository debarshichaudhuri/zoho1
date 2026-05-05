package com.qmanage.app.data.repository

import android.content.Context
import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.Preferences
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map

private val Context.dataStore: DataStore<Preferences> by preferencesDataStore("qmanage_prefs")

class PrefsRepository(private val context: Context) {
    companion object {
        private val KEY_TOKEN      = stringPreferencesKey("auth_token")
        private val KEY_SERVER_URL = stringPreferencesKey("server_url")
        private val KEY_USERNAME   = stringPreferencesKey("username")
        const val DEFAULT_URL = "http://192.168.29.37:9741/api/"
    }

    val token: Flow<String> = context.dataStore.data.map { it[KEY_TOKEN] ?: "" }
    val serverUrl: Flow<String> = context.dataStore.data.map { it[KEY_SERVER_URL] ?: DEFAULT_URL }
    val username: Flow<String> = context.dataStore.data.map { it[KEY_USERNAME] ?: "" }

    suspend fun saveSession(token: String, username: String) {
        context.dataStore.edit {
            it[KEY_TOKEN]    = token
            it[KEY_USERNAME] = username
        }
    }

    suspend fun setServerUrl(url: String) {
        context.dataStore.edit { it[KEY_SERVER_URL] = url }
    }

    suspend fun clearSession() {
        context.dataStore.edit {
            it.remove(KEY_TOKEN)
        }
    }
}
