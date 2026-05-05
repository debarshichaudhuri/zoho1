package com.qmanage.app.viewmodel

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.qmanage.app.data.api.ApiClient
import com.qmanage.app.data.model.*
import com.qmanage.app.data.preferences.ThemePreferences
import com.qmanage.app.data.repository.PrefsRepository
import com.qmanage.app.data.repository.QManageRepository
import com.qmanage.app.data.repository.Result
import kotlinx.coroutines.flow.*
import kotlinx.coroutines.launch

// ============================================================
// UI States
// ============================================================
data class AuthState(
    val isLoggedIn: Boolean = false,
    val isLoading: Boolean = false,
    val error: String = ""
)

data class DashboardState(
    val data: DashboardResponse? = null,
    val isLoading: Boolean = false,
    val error: String = ""
)

data class ListState<T>(
    val items: List<T> = emptyList(),
    val isLoading: Boolean = false,
    val error: String = "",
    val page: Int = 1
)

// ============================================================
// Shared App ViewModel
// ============================================================
class AppViewModel(application: Application) : AndroidViewModel(application) {
    private val repo = QManageRepository()
    val prefs = PrefsRepository(application)
    val themePrefs = ThemePreferences(application)

    // ---- Auth ----
    private val _auth = MutableStateFlow(AuthState())
    val auth: StateFlow<AuthState> = _auth.asStateFlow()

    // ---- Dashboard ----
    private val _dashboard = MutableStateFlow(DashboardState())
    val dashboard: StateFlow<DashboardState> = _dashboard.asStateFlow()

    // ---- Contacts ----
    private val _contacts = MutableStateFlow(ListState<Contact>())
    val contacts: StateFlow<ListState<Contact>> = _contacts.asStateFlow()

    // ---- Invoices ----
    private val _invoices = MutableStateFlow(ListState<Invoice>())
    val invoices: StateFlow<ListState<Invoice>> = _invoices.asStateFlow()

    // ---- Expenses ----
    private val _expenses = MutableStateFlow(ListState<Expense>())
    val expenses: StateFlow<ListState<Expense>> = _expenses.asStateFlow()

    // ---- Bills ----
    private val _bills = MutableStateFlow(ListState<Bill>())
    val bills: StateFlow<ListState<Bill>> = _bills.asStateFlow()

    // ---- Items ----
    private val _items = MutableStateFlow(ListState<Item>())
    val items: StateFlow<ListState<Item>> = _items.asStateFlow()

    // ---- Quotes ----
    private val _quotes = MutableStateFlow(ListState<Quote>())
    val quotes: StateFlow<ListState<Quote>> = _quotes.asStateFlow()

    // ---- Deals ----
    private val _deals = MutableStateFlow(ListState<Deal>())
    val deals: StateFlow<ListState<Deal>> = _deals.asStateFlow()

    // Toast / snackbar events
    private val _toast = MutableSharedFlow<String>()
    val toast: SharedFlow<String> = _toast.asSharedFlow()

    init {
        // Restore session from prefs
        viewModelScope.launch {
            combine(prefs.token, prefs.serverUrl) { token, url -> Pair(token, url) }
                .collect { (token, url) ->
                    ApiClient.configure(url, token)
                    _auth.value = AuthState(isLoggedIn = token.isNotEmpty())
                }
        }
    }

    // ---- Auth ----
    fun login(username: String, password: String) {
        viewModelScope.launch {
            _auth.value = _auth.value.copy(isLoading = true, error = "")
            when (val r = repo.login(username, password)) {
                is Result.Success -> {
                    ApiClient.setToken(r.data.token)
                    prefs.saveSession(r.data.token, username)
                    _auth.value = AuthState(isLoggedIn = true)
                    loadDashboard()
                }
                is Result.Error -> _auth.value = AuthState(error = r.message)
            }
        }
    }

    fun logout() {
        viewModelScope.launch {
            repo.logout()
            ApiClient.clearToken()
            prefs.clearSession()
            _auth.value = AuthState(isLoggedIn = false)
        }
    }

    // ---- Dashboard ----
    fun loadDashboard() {
        viewModelScope.launch {
            _dashboard.value = _dashboard.value.copy(isLoading = true)
            when (val r = repo.getDashboard()) {
                is Result.Success -> _dashboard.value = DashboardState(data = r.data)
                is Result.Error   -> _dashboard.value = DashboardState(error = r.message)
            }
        }
    }

    // ---- Contacts ----
    fun loadContacts(reset: Boolean = false) {
        viewModelScope.launch {
            val offset = if (reset) 0 else (_contacts.value.items.size)
            _contacts.value = _contacts.value.copy(isLoading = true)
            when (val r = repo.getContacts(offset = offset)) {
                is Result.Success -> _contacts.value = ListState(
                    items = if (reset) r.data else _contacts.value.items + r.data
                )
                is Result.Error -> _contacts.value = _contacts.value.copy(isLoading = false, error = r.message)
            }
        }
    }

    fun createContact(displayName: String, type: String, email: String, phone: String, company: String) {
        viewModelScope.launch {
            val req = CreateContactRequest(displayName, type, email, phone, companyName = company)
            when (val r = repo.createContact(req)) {
                is Result.Success -> { loadContacts(reset = true); _toast.emit("Contact created") }
                is Result.Error   -> _toast.emit("Error: ${r.message}")
            }
        }
    }

    fun deleteContact(id: Long) {
        viewModelScope.launch {
            when (val r = repo.deleteContact(id)) {
                is Result.Success -> { loadContacts(reset = true); _toast.emit("Contact deleted") }
                is Result.Error   -> _toast.emit("Error: ${r.message}")
            }
        }
    }

    // ---- Invoices ----
    fun loadInvoices(reset: Boolean = false) {
        viewModelScope.launch {
            val offset = if (reset) 0 else _invoices.value.items.size
            _invoices.value = _invoices.value.copy(isLoading = true)
            when (val r = repo.getInvoices(offset = offset)) {
                is Result.Success -> _invoices.value = ListState(
                    items = if (reset) r.data else _invoices.value.items + r.data
                )
                is Result.Error -> _invoices.value = _invoices.value.copy(isLoading = false, error = r.message)
            }
        }
    }

    fun createInvoice(invoiceNum: String, customerId: Long, totalRupees: Double, date: Long, notes: String = "") {
        viewModelScope.launch {
            val totalPaisa = (totalRupees * 100).toLong()
            val req = CreateInvoiceRequest(invoiceNum, customerId, totalPaisa, totalPaisa, date = date, notes = notes)
            when (val r = repo.createInvoice(req)) {
                is Result.Success -> { loadInvoices(reset = true); _toast.emit("Invoice created") }
                is Result.Error   -> _toast.emit("Error: ${r.message}")
            }
        }
    }

    // ---- Expenses ----
    fun loadExpenses(reset: Boolean = false) {
        viewModelScope.launch {
            val offset = if (reset) 0 else _expenses.value.items.size
            _expenses.value = _expenses.value.copy(isLoading = true)
            when (val r = repo.getExpenses(offset = offset)) {
                is Result.Success -> _expenses.value = ListState(
                    items = if (reset) r.data else _expenses.value.items + r.data
                )
                is Result.Error -> _expenses.value = _expenses.value.copy(isLoading = false, error = r.message)
            }
        }
    }

    fun createExpense(category: String, amountRupees: Double, date: Long, notes: String = "") {
        viewModelScope.launch {
            val amountPaisa = (amountRupees * 100).toLong()
            val req = CreateExpenseRequest(category, amountPaisa, date, notes)
            when (val r = repo.createExpense(req)) {
                is Result.Success -> { loadExpenses(reset = true); _toast.emit("Expense recorded") }
                is Result.Error   -> _toast.emit("Error: ${r.message}")
            }
        }
    }

    // ---- Bills ----
    fun loadBills(reset: Boolean = false) {
        viewModelScope.launch {
            val offset = if (reset) 0 else _bills.value.items.size
            _bills.value = _bills.value.copy(isLoading = true)
            when (val r = repo.getBills(offset = offset)) {
                is Result.Success -> _bills.value = ListState(
                    items = if (reset) r.data else _bills.value.items + r.data
                )
                is Result.Error -> _bills.value = _bills.value.copy(isLoading = false, error = r.message)
            }
        }
    }

    // ---- Items ----
    fun loadItems(reset: Boolean = false) {
        viewModelScope.launch {
            val offset = if (reset) 0 else _items.value.items.size
            _items.value = _items.value.copy(isLoading = true)
            when (val r = repo.getItems(offset = offset)) {
                is Result.Success -> _items.value = ListState(
                    items = if (reset) r.data else _items.value.items + r.data
                )
                is Result.Error -> _items.value = _items.value.copy(isLoading = false, error = r.message)
            }
        }
    }

    // ---- Quotes ----
    fun loadQuotes(reset: Boolean = false) {
        viewModelScope.launch {
            val offset = if (reset) 0 else _quotes.value.items.size
            _quotes.value = _quotes.value.copy(isLoading = true)
            when (val r = repo.getQuotes(offset = offset)) {
                is Result.Success -> _quotes.value = ListState(
                    items = if (reset) r.data else _quotes.value.items + r.data
                )
                is Result.Error -> _quotes.value = _quotes.value.copy(isLoading = false, error = r.message)
            }
        }
    }

    fun createQuote(quoteNum: String, customerId: Long, totalRupees: Double, notes: String = "") {
        viewModelScope.launch {
            val totalPaisa = (totalRupees * 100).toLong()
            val req = CreateQuoteRequest(quoteNum, customerId, totalPaisa, totalPaisa, System.currentTimeMillis() / 1000, notes)
            when (val r = repo.createQuote(req)) {
                is Result.Success -> { loadQuotes(reset = true); _toast.emit("Quote created") }
                is Result.Error   -> _toast.emit("Error: ${r.message}")
            }
        }
    }

    // ---- Deals ----
    fun loadDeals(reset: Boolean = false) {
        viewModelScope.launch {
            _deals.value = _deals.value.copy(isLoading = true)
            when (val r = repo.getDeals()) {
                is Result.Success -> _deals.value = ListState(items = r.data.deals)
                is Result.Error   -> _deals.value = _deals.value.copy(isLoading = false, error = r.message)
            }
        }
    }

    fun createDeal(title: String, contactId: Long = 0, amountRupees: Double = 0.0, notes: String = "") {
        viewModelScope.launch {
            val req = CreateDealRequest(title, contactId, "contacted", (amountRupees * 100).toLong(), 50, notes)
            when (val r = repo.createDeal(req)) {
                is Result.Success -> { loadDeals(reset = true); _toast.emit("Deal created") }
                is Result.Error   -> _toast.emit("Error: ${r.message}")
            }
        }
    }

    // ---- Server URL update ----
    fun updateServerUrl(url: String) {
        viewModelScope.launch {
            prefs.setServerUrl(url)
            ApiClient.configure(url, "")
            prefs.clearSession()
            _auth.value = AuthState(isLoggedIn = false)
            _toast.emit("Server URL updated — please login again")
        }
    }
}

// ============================================================
// Money formatting helpers (Kotlin)
// ============================================================
fun Long.toRupees(): Double = this / 100.0
fun Long.formatRupees(): String = "₹%,.2f".format(this / 100.0)
fun Double.toPaisa(): Long = (this * 100).toLong()
