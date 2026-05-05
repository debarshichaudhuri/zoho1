/* Q Manage — Banking Bridge Module (Production Grade) */
const BankingModule = {
  async renderList() {
    return `
      <div class="page-header">
        <h1 class="page-title">Banking Bridge</h1>
        <div class="page-actions">
          <button class="btn btn-secondary" onclick="BankingModule.reconcile()">
            <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="23 4 23 10 17 10"/><polyline points="1 20 1 14 7 14"/><path d="M3.51 9a9 9 0 0 1 14.85-3.36L23 10M1 14l4.64 4.36A9 9 0 0 0 20.49 15"/></svg>
            Auto-Reconcile
          </button>
        </div>
      </div>

      <div class="stats-grid" style="margin-bottom:24px">
        <div class="stat-card blue">
          <span class="stat-label">Bank Balance</span>
          <span class="stat-value">₹4,52,000</span>
          <span class="stat-change">SBI Current A/c</span>
        </div>
        <div class="stat-card green">
          <span class="stat-label">Matched</span>
          <span class="stat-value">0</span>
          <span class="stat-change up">Auto-reconciled</span>
        </div>
        <div class="stat-card yellow">
          <span class="stat-label">Unmatched</span>
          <span class="stat-value">0</span>
          <span class="stat-change">Pending review</span>
        </div>
      </div>

      <div class="card" style="margin-bottom:24px">
        <div class="card-header"><span class="card-title">SMS Transaction Parser</span></div>
        <p style="font-size:.8rem;color:var(--text-s);margin-bottom:12px">
          Paste an Indian Bank SMS (HDFC, ICICI, SBI, Kotak) below. The C engine will extract amount, type, account, and UTR reference automatically.
        </p>
        <textarea id="sms-input" class="form-control" style="min-height:90px;margin-bottom:12px;font-family:monospace;font-size:.8rem" placeholder="Example: Your A/c XX1234 has been credited with Rs. 5,000.00 on 28-APR-26 by UPI ref 611823490123."></textarea>
        <div style="display:flex;gap:8px">
          <button class="btn btn-primary" onclick="BankingModule.parseSMS()">
            <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M22 2L11 13"/><polygon points="22 2 15 22 11 13 2 9 22 2"/></svg>
            Process SMS
          </button>
          <button class="btn btn-ghost" onclick="document.getElementById('sms-input').value=''">Clear</button>
        </div>
      </div>

      <div class="card">
        <div class="card-header">
          <span class="card-title">Recent Bank Transactions</span>
          <button class="btn btn-ghost btn-sm" onclick="BankingModule.loadTransactions()">Refresh</button>
        </div>
        <div id="bank-tx-list">
          <div class="empty-state" style="padding:40px">
            <p>No bank transactions imported yet. Parse an SMS or import a statement to begin.</p>
          </div>
        </div>
      </div>
    `;
  },

  async parseSMS() {
    const text = document.getElementById('sms-input')?.value;
    if (!text || !text.trim()) {
      QManage.toast('Please paste a bank SMS first', 'error');
      return;
    }

    try {
      const res = await API.post('/banking/sms', { sms_text: text.trim() });
      QManage.toast(`Parsed ${res.type}: ₹${Number(res.amount_parsed).toLocaleString('en-IN')} | UTR: ${res.utr || 'N/A'}`, 'success');
      document.getElementById('sms-input').value = '';
      this.loadTransactions();
    } catch (e) {
      QManage.toast('Parse failed: ' + e.message, 'error');
    }
  },

  async reconcile() {
    QManage.toast('Running Auto-Reconciliation Engine...', 'info');
    try {
      await API.post('/banking/reconcile', {});
      QManage.toast('Reconciliation Complete!', 'success');
      this.loadTransactions();
    } catch (e) {
      QManage.toast('Reconciliation error: ' + e.message, 'error');
    }
  },

  async loadTransactions() {
    const list = document.getElementById('bank-tx-list');
    if (!list) return;

    try {
      const entries = await API.get('/ledger');
      if (!entries || entries.length === 0) {
        list.innerHTML = '<div class="empty-state" style="padding:40px"><p>No transactions in ledger.</p></div>';
        return;
      }
      const fmt = (n) => '₹' + Number(n).toLocaleString('en-IN', { minimumFractionDigits: 2 });
      list.innerHTML = `
        <table class="data-table">
          <thead><tr><th>Date</th><th>Description</th><th>Debit</th><th>Credit</th><th>Status</th></tr></thead>
          <tbody>
            ${entries.slice(0, 10).map(e => `
              <tr>
                <td>${new Date((e.entry_date || 0) * 1000).toLocaleDateString('en-IN')}</td>
                <td style="font-weight:500">${e.entry_memo || e.memo || '-'}</td>
                <td style="color:var(--red);font-weight:500">${e.debit > 0 ? fmt(e.debit) : ''}</td>
                <td style="color:var(--green);font-weight:500">${e.credit > 0 ? fmt(e.credit) : ''}</td>
                <td><span class="badge badge-paid">SYNCED</span></td>
              </tr>
            `).join('')}
          </tbody>
        </table>
      `;
    } catch (e) {
      list.innerHTML = '<div class="empty-state" style="padding:20px"><p>Backend offline — cannot load transactions.</p></div>';
    }
  }
};
