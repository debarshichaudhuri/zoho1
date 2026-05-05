/* Q Manage — Main Application Router & Shell (Production Grade) */
const QManage = {
  async init() {
    console.log("%c Q Manage — Fintech Engine v2.0 ", "background: #1a73e8; color: #fff; font-weight: bold; padding: 4px 8px; border-radius: 4px;");

    // Populate saved server URL in login screen
    const savedServer = API.BASE_URL;
    const serverInput = document.getElementById('login-server-url');
    if (serverInput) serverInput.value = savedServer;

    // Show login screen if not authenticated
    if (!Auth.isLoggedIn()) {
      this.showLoginScreen();
      return;
    }

    await this.startApp();
  },

  showLoginScreen() {
    document.getElementById('app').style.display = 'none';
    document.getElementById('login-screen').style.display = 'flex';
  },

  async handleLogin(e) {
    e.preventDefault();
    const username = document.getElementById('login-username').value;
    const password = document.getElementById('login-password').value;
    const btn = document.getElementById('login-btn');
    const errDiv = document.getElementById('login-error');

    btn.disabled = true;
    btn.textContent = 'Signing in...';
    errDiv.style.display = 'none';

    console.log('handleLogin called:', { username });

    try {
      await API.login(username, password);
      console.log('Login successful');
      document.getElementById('login-screen').style.display = 'none';
      document.getElementById('app').style.display = '';
      await this.startApp();
    } catch (err) {
      console.error('Login error in handleLogin:', err);
      errDiv.textContent = err.message || 'Login failed - check console for details';
      errDiv.style.display = 'block';
      btn.disabled = false;
      btn.textContent = 'Sign In';
    }
  },

  updateServerUrl() {
    const url = document.getElementById('login-server-url').value.trim();
    if (url) {
      const normalized = API.setServer(url);
      document.getElementById('login-server-url').value = normalized;
      this.toast('Server URL updated', 'success');
    }
  },

  async startApp() {
    // 1. Initialize API & Backend Check
    const isBackendReady = await API.checkHealth();
    if (isBackendReady) {
      this.toast('Connected to C Engine (SQLite WAL)', 'success');
    } else {
      this.toast('Backend not detected — check server', 'warning');
    }

    // 2. Setup UI Components
    this.setupUI();

    // 3. Router
    window.addEventListener('hashchange', () => this.route());

    // 4. Initial Route
    if (!location.hash || location.hash === '#/') location.hash = '#/dashboard';
    else this.route();

    // 5. Init chatbot (fires after auth is confirmed)
    document.dispatchEvent(new Event('qmanage:loggedin'));
  },

  setupUI() {
    // Sidebar Collapse
    document.getElementById('sidebar-toggle')?.addEventListener('click', () => {
      document.getElementById('sidebar').classList.toggle('collapsed');
    });

    // Mobile Menu
    document.getElementById('mobile-menu-btn')?.addEventListener('click', () => {
      document.getElementById('sidebar').classList.toggle('mobile-open');
    });

    // Theme Switcher
    const savedTheme = localStorage.getItem('q_theme') || 'light';
    document.documentElement.setAttribute('data-theme', savedTheme);

    document.getElementById('theme-toggle')?.addEventListener('click', () => {
      const html = document.documentElement;
      const next = html.getAttribute('data-theme') === 'dark' ? 'light' : 'dark';
      html.setAttribute('data-theme', next);
      localStorage.setItem('q_theme', next);
    });

    // Quick Create Modal
    document.getElementById('quick-create-btn')?.addEventListener('click', () => {
      document.getElementById('quick-create-modal').style.display = 'flex';
    });

    // Submenu logic
    document.querySelectorAll('.has-submenu > .nav-link').forEach(link => {
      link.addEventListener('click', (e) => {
        e.preventDefault();
        link.parentElement.classList.toggle('open');
      });
    });

    // Close mobile sidebar on outside click
    document.addEventListener('click', (e) => {
      const sidebar = document.getElementById('sidebar');
      const mobileBtn = document.getElementById('mobile-menu-btn');
      if (window.innerWidth <= 768 && sidebar?.classList.contains('mobile-open')) {
        if (!sidebar.contains(e.target) && !mobileBtn?.contains(e.target)) {
          sidebar.classList.remove('mobile-open');
        }
      }
    });

    // Keyboard shortcut: Ctrl+K for search
    document.addEventListener('keydown', (e) => {
      if ((e.ctrlKey || e.metaKey) && e.key === 'k') {
        e.preventDefault();
        document.getElementById('global-search')?.focus();
      }
    });

    // Execute global search on Enter
    document.getElementById('global-search')?.addEventListener('keyup', (e) => {
      if (e.key === 'Enter') {
        const q = e.target.value.trim();
        const mode = document.getElementById('search-mode')?.value || 'fts5';
        if (q) window.location.hash = '#/search?q=' + encodeURIComponent(q) + '&mode=' + encodeURIComponent(mode);
      }
    });
  },

  async route() {
    if (!localStorage.getItem('qm_setup_complete') && location.hash !== '#/setup') {
      window.location.hash = '#/setup';
      return;
    }

    const hash = location.hash.replace('#', '') || '/dashboard';
    const parts = hash.split('?')[0].split('/').filter(Boolean);
    const module = parts[0] || 'dashboard';
    const action = parts[1];

    // Parse query params
    const queryStr = hash.split('?')[1];
    const params = {};
    if (queryStr) {
      queryStr.split('&').forEach(pair => {
        const [k, v] = pair.split('=');
        params[k] = decodeURIComponent(v);
      });
    }

    // Update active nav
    document.querySelectorAll('.nav-item, .nav-submenu li').forEach(el => el.classList.remove('active'));
    let navTarget = document.querySelector(`[data-route="${module}"]`);
    if (!navTarget && action && isNaN(action) && action !== 'new' && action !== 'edit') {
      navTarget = document.querySelector(`[data-route="${action}"]`);
    }
    if (navTarget) {
      navTarget.classList.add('active');
      if (navTarget.parentElement?.classList.contains('nav-submenu')) {
        navTarget.parentElement.parentElement.classList.add('open', 'active');
      }
    }

    // Render content
    const contentArea = document.getElementById('page-content');
    contentArea.innerHTML = '<div style="display:flex;flex-direction:column;align-items:center;justify-content:center;min-height:200px"><div class="spinner"></div><p style="margin-top:12px;font-size:.8rem;color:var(--text-xs)">Loading...</p></div>';

    try {
      let html = '';

      switch (module) {
        case 'setup':
          html = await SetupModule.render();
          break;
        case 'dashboard':
          html = await DashboardModule.render();
          break;
        case 'ledger':
        case 'manual-journals':
          html = await this.renderLedger();
          break;
        case 'contacts':
          if (action === 'new' || action === 'edit') html = await ContactsModule.renderForm(parts[2] || 'new', params);
          else if (action && !isNaN(action)) html = await ContactsModule.renderView(action);
          else html = await ContactsModule.renderList(params.type);
          break;
        case 'items':
          if (action === 'new' || action === 'edit') html = await ItemsModule.renderForm(parts[2] || 'new');
          else html = await ItemsModule.renderList();
          break;
        case 'invoices':
          if (action === 'new' || action === 'edit') html = await InvoicesModule.renderForm(parts[2] || 'new');
          else if (action && !isNaN(action)) html = await InvoicesModule.renderView(action);
          else html = await InvoicesModule.renderList();
          break;
        case 'expenses':
          if (action === 'new' || action === 'edit') html = await ExpensesModule.renderForm(parts[2] || 'new');
          else html = await ExpensesModule.renderList();
          break;

        // ---- NEW ERP MODULES ----
        case 'bills':
          if (action === 'new' || action === 'edit') html = await BillsModule.renderForm(parts[2] || 'new');
          else html = await BillsModule.renderList();
          break;
        case 'quotes':
        case 'estimates':
          if (action === 'new' || action === 'edit') html = await QuotesModule.renderForm(parts[2] || 'new');
          else html = await QuotesModule.renderList();
          break;
        case 'payments':
          html = await PaymentsModule.renderList();
          break;
        case 'credit-notes':
          if (action === 'new') html = await CreditNotesModule.showCreateForm();
          else html = await CreditNotesModule.renderList();
          break;
        case 'vendor-credits':
          if (action === 'new') html = await VendorCreditsModule.showCreateForm();
          else html = await VendorCreditsModule.renderList();
          break;
        case 'purchase-orders':
          if (action === 'new') html = await PurchaseOrdersModule.showCreateForm();
          else html = await PurchaseOrdersModule.renderList();
          break;
        case 'inventory':
          html = await InventoryModule.renderList();
          break;
        case 'crm':
        case 'deals':
          html = await CRMModule.renderList();
          break;
        case 'leads':
          if (action === 'new') { LeadsModule.renderForm(); return; }
          html = await LeadsModule.renderList();
          break;
        case 'follow-ups':
          html = await FollowUpsModule.renderList();
          break;
        case 'search':
          html = await SearchModule.renderList(params);
          break;

        // ---- REPORTS (ENHANCED) ----
        case 'reports':
          if (action === 'profit-and-loss') html = await ReportsModule.renderProfitAndLoss();
          else if (action === 'balance-sheet') html = await ReportsModule.renderBalanceSheet();
          else if (action === 'trial-balance') html = await ReportsModule.renderTrialBalance();
          else if (action === 'cash-flow') html = await ReportsModule.renderCashFlow();
          else if (action === 'aging') html = await ReportsModule.renderAging();
          else if (action === 'sales-by-customer') html = await ReportsModule.renderSalesByCustomer();
          else if (action === 'expense-by-category') html = await ReportsModule.renderExpenseByCategory();
          else if (action === 'gst-summary') html = await ReportsModule.renderGSTSummary();
          else html = await ReportsModule.renderList();
          break;

        case 'settings':
          html = await SettingsModule.render();
          // afterRender wires up tab switching; must run after innerHTML is set
          setTimeout(() => SettingsModule.afterRender(), 0);
          break;
        case 'banking':
        case 'bank-accounts':
        case 'import-statement':
          html = await BankingModule.renderList();
          break;
        case 'backup':
          html = await BackupModule.render();
          break;
        case 'chart-of-accounts':
          html = await this.renderChartOfAccounts();
          break;

        // ---- NEW MODULES (Phase 2) ----
        case 'credit-notes':
          html = await CreditNotesModule.renderList();
          break;
        case 'vendor-credits':
          html = await VendorCreditsModule.renderList();
          break;
        case 'purchase-orders':
          html = await PurchaseOrdersModule.renderList();
          break;
        case 'inventory':
          html = await InventoryModule.renderList();
          break;
        case 'audit':
        case 'audit-trail':
          html = await AuditModule.render();
          break;

        case 'sales':
          location.hash = '#/invoices'; return;
        case 'purchases':
          location.hash = '#/bills'; return;
        default:
          html = `<div class="empty-state">
                    <svg viewBox="0 0 24 24" width="64" height="64" fill="none" stroke="currentColor" stroke-width="1" style="margin-bottom:16px;opacity:.3"><path d="M13 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V9z"/><polyline points="13 2 13 9 20 9"/></svg>
                    <h3>Module: ${module}</h3>
                    <p>This module is under development for the production release.</p>
                    <a href="#/dashboard" class="btn btn-primary">Back to Dashboard</a>
                  </div>`;
      }

      if (typeof html === 'string' && html !== '') {
        contentArea.innerHTML = html;
        // Post-render chart initialization
        if (module === 'dashboard') setTimeout(() => DashboardModule.afterRender && DashboardModule.afterRender(), 0);
        if (module === 'reports' && action) setTimeout(() => ReportsModule.afterRender && ReportsModule.afterRender(action), 0);
      }

    } catch (e) {
      console.error('Routing Error:', e);
      contentArea.innerHTML = `<div class="empty-state" style="color:var(--red)"><h3>Error loading page</h3><p>${e.message}</p><a href="#/dashboard" class="btn btn-secondary" style="margin-top:12px">Back to Dashboard</a></div>`;
    }
  },

  async renderLedger() {
    let entries = [];
    let accounts = [];
    try { entries = await API.get('/ledger'); } catch (e) { }
    try { accounts = await API.get('/accounts'); } catch (e) { }

    const fmt = (n) => '₹' + Number(n || 0).toLocaleString('en-IN', { minimumFractionDigits: 2 });
    const dt = (ts) => ts ? new Date(ts * 1000).toLocaleDateString('en-IN', { day: '2-digit', month: 'short', year: 'numeric' }) : '-';

    return `
      <div class="page-header">
        <h1 class="page-title">General Ledger</h1>
        <div class="page-actions"><button class="btn btn-primary" onclick="QManage.toggleJournalForm()">+ New Journal Entry</button></div>
      </div>

      <!-- Manual Journal Entry Form -->
      <div class="card" id="journal-form" style="display:none;margin-bottom:20px;max-width:900px">
        <div class="card-header"><span class="card-title">Manual Journal Entry</span></div>
        <div class="form-section">
          <div class="form-row">
            <div class="form-group">
              <label class="form-label required">Date</label>
              <input class="form-control" type="date" id="je-date" value="${new Date().toISOString().split('T')[0]}">
            </div>
            <div class="form-group">
              <label class="form-label required">Memo / Description</label>
              <input class="form-control" id="je-memo" placeholder="e.g. Cash sale, Owner contribution">
            </div>
          </div>
          <table class="data-table" id="je-lines-table">
            <thead><tr><th>Account</th><th style="width:160px">Debit (₹)</th><th style="width:160px">Credit (₹)</th><th style="width:40px"></th></tr></thead>
            <tbody id="je-lines">
              <tr>
                <td><select class="form-control je-account">${accounts.map(a => `<option value="${a.id}">${a.code} — ${a.name}</option>`).join('')}</select></td>
                <td><input class="form-control je-debit" type="number" step="0.01" value="0"></td>
                <td><input class="form-control je-credit" type="number" step="0.01" value="0"></td>
                <td></td>
              </tr>
              <tr>
                <td><select class="form-control je-account">${accounts.map(a => `<option value="${a.id}">${a.code} — ${a.name}</option>`).join('')}</select></td>
                <td><input class="form-control je-debit" type="number" step="0.01" value="0"></td>
                <td><input class="form-control je-credit" type="number" step="0.01" value="0"></td>
                <td></td>
              </tr>
            </tbody>
          </table>
          <div style="display:flex;gap:10px;margin-top:12px;align-items:center">
            <button class="btn btn-secondary" onclick="QManage.addJournalLine()">+ Add Line</button>
            <div style="flex:1"></div>
            <span id="je-balance-status" style="font-size:.82rem;font-weight:600"></span>
            <button class="btn btn-primary" onclick="QManage.submitJournalEntry()">Post to Ledger</button>
          </div>
        </div>
      </div>

      <div class="table-container">
        ${entries.length ? `
        <table class="data-table">
          <thead>
            <tr><th>Date</th><th>Code</th><th>Account</th><th>Memo</th><th style="text-align:right">Debit</th><th style="text-align:right">Credit</th></tr>
          </thead>
          <tbody>
            ${entries.map(l => `
              <tr>
                <td>${dt(l.entry_date)}</td>
                <td style="font-family:monospace;font-size:.72rem;color:var(--text-xs)">${l.account_code || ''}</td>
                <td style="font-weight:600">${l.account_name || '-'}</td>
                <td style="color:var(--text-s)">${l.memo || '-'}</td>
                <td style="text-align:right;color:var(--red);font-weight:500">${l.debit > 0 ? fmt(l.debit) : ''}</td>
                <td style="text-align:right;color:var(--green);font-weight:500">${l.credit > 0 ? fmt(l.credit) : ''}</td>
              </tr>
            `).join('')}
          </tbody>
        </table>` : '<div class="empty-state"><h3>No journal entries</h3><p>Record your first transaction to see it here.</p></div>'}
      </div>
    `;
  },

  toggleJournalForm() {
    const form = document.getElementById('journal-form');
    if (form) form.style.display = form.style.display === 'none' ? 'block' : 'none';
  },

  addJournalLine() {
    const tbody = document.getElementById('je-lines');
    if (!tbody) return;
    const firstSelect = tbody.querySelector('.je-account');
    if (!firstSelect) return;
    const row = document.createElement('tr');
    row.innerHTML = `
      <td><select class="form-control je-account">${firstSelect.innerHTML}</select></td>
      <td><input class="form-control je-debit" type="number" step="0.01" value="0"></td>
      <td><input class="form-control je-credit" type="number" step="0.01" value="0"></td>
      <td><button class="btn btn-ghost" style="color:var(--red);padding:4px" onclick="this.closest('tr').remove()">✕</button></td>
    `;
    tbody.appendChild(row);
  },

  async submitJournalEntry() {
    const memo = document.getElementById('je-memo')?.value?.trim();
    const dateStr = document.getElementById('je-date')?.value;
    if (!memo) { this.toast('Memo is required', 'error'); return; }

    const rows = document.querySelectorAll('#je-lines tr');
    const lines = [];
    let totalDr = 0, totalCr = 0;

    rows.forEach(row => {
      const account_id = parseInt(row.querySelector('.je-account')?.value) || 0;
      const debit = Math.round(Number(row.querySelector('.je-debit')?.value || 0) * 100);
      const credit = Math.round(Number(row.querySelector('.je-credit')?.value || 0) * 100);
      if (account_id && (debit > 0 || credit > 0)) {
        lines.push({ account_id, debit, credit });
        totalDr += debit;
        totalCr += credit;
      }
    });

    if (lines.length < 2) { this.toast('At least 2 lines required', 'error'); return; }
    if (totalDr !== totalCr) {
      this.toast(`Unbalanced! Debit: ₹${(totalDr / 100).toFixed(2)} ≠ Credit: ₹${(totalCr / 100).toFixed(2)}`, 'error');
      return;
    }

    try {
      await API.post('/ledger', {
        memo,
        date: Math.floor(new Date(dateStr).getTime() / 1000),
        lines
      });
      this.toast(`Journal entry "${memo}" posted (₹${(totalDr / 100).toFixed(2)})`, 'success');
      this.route();
    } catch (e) {
      this.toast('Failed: ' + e.message, 'error');
    }
  },

  async renderChartOfAccounts() {
    let accounts = [];
    try {
      accounts = await API.get('/accounts');
    } catch (e) { /* offline */ }

    const typeLabels = { 1: 'Asset', 2: 'Liability', 3: 'Equity', 4: 'Income', 5: 'Expense' };
    const typeColors = { 1: 'sent', 2: 'overdue', 3: 'partial', 4: 'paid', 5: 'draft' };

    return `
      <div class="page-header">
        <h1 class="page-title">Chart of Accounts</h1>
      </div>
      <div class="table-container">
        <table class="data-table">
          <thead><tr><th>Code</th><th>Account Name</th><th>Type</th><th>System</th></tr></thead>
          <tbody>
            ${accounts.map(a => `
              <tr>
                <td style="font-weight:600;color:var(--primary)">${a.code || '-'}</td>
                <td>${a.name}</td>
                <td><span class="badge badge-${typeColors[a.type] || 'draft'}">${typeLabels[a.type] || 'Unknown'}</span></td>
                <td>${a.is_system ? '✓' : ''}</td>
              </tr>
            `).join('')}
          </tbody>
        </table>
      </div>
    `;
  },

  toast(msg, type = 'info') {
    const container = document.getElementById('toast-container');
    if (!container) return;
    const toast = document.createElement('div');
    toast.className = `toast ${type}`;

    let icon = '';
    if (type === 'success') icon = '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><path d="M22 11.08V12a10 10 0 1 1-5.93-9.14"/><polyline points="22 4 12 14.01 9 11.01"/></svg>';
    else if (type === 'error') icon = '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><circle cx="12" cy="12" r="10"/><line x1="15" y1="9" x2="9" y2="15"/><line x1="9" y1="9" x2="15" y2="15"/></svg>';
    else icon = '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><circle cx="12" cy="12" r="10"/><line x1="12" y1="16" x2="12" y2="12"/><line x1="12" y1="8" x2="12.01" y2="8"/></svg>';

    toast.innerHTML = `${icon}<span>${msg}</span>`;
    container.appendChild(toast);

    setTimeout(() => {
      toast.style.opacity = '0';
      toast.style.transform = 'translateX(40px)';
      toast.style.transition = 'all .3s ease';
      setTimeout(() => toast.remove(), 300);
    }, 3500);
  },

  closeModal(id) {
    const el = document.getElementById(id);
    if (el) el.style.display = 'none';
  }
};

document.addEventListener('DOMContentLoaded', () => QManage.init());
