/* Q Manage — Expenses Module (C API + Ledger Integrated) */
const ExpensesModule = {
  async renderList() {
    let expenses = [];
    try { expenses = await API.get('/expenses'); } catch(e) { /* offline */ }

    const fmt = (n) => '₹' + Number(n || 0).toLocaleString('en-IN', { minimumFractionDigits: 2 });
    const dt = (ts) => ts ? new Date(ts * 1000).toLocaleDateString('en-IN', { day: '2-digit', month: 'short', year: 'numeric' }) : '-';

    return `
      <div class="page-header">
        <h1 class="page-title">Expenses</h1>
        <div class="page-actions">
          <button class="btn btn-primary" onclick="ExpensesModule.showCreateForm()">
            <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/></svg>
            Record Expense
          </button>
        </div>
      </div>
      <div class="table-container">
        ${expenses.length ? `
        <table class="data-table">
          <thead><tr><th>Date</th><th>Description</th><th style="text-align:right">Amount</th></tr></thead>
          <tbody>
            ${expenses.map(exp => `
              <tr>
                <td>${dt(exp.entry_date)}</td>
                <td style="font-weight:500">${exp.memo || 'General Expense'}</td>
                <td style="text-align:right;font-weight:600;color:var(--red)">${fmt(exp.amount)}</td>
              </tr>
            `).join('')}
          </tbody>
        </table>` : `
        <div class="empty-state">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" style="width:64px;height:64px"><rect x="1" y="4" width="22" height="16" rx="2" ry="2"/><line x1="1" y1="10" x2="23" y2="10"/></svg>
          <h3>No expenses recorded</h3>
          <p>Track your business spending. Every expense auto-posts to the double-entry ledger.</p>
          <button onclick="ExpensesModule.showCreateForm()" class="btn btn-primary">+ Record Expense</button>
        </div>`}
      </div>`;
  },

  showCreateForm() {
    const content = document.getElementById('page-content');
    content.innerHTML = `
      <div class="page-header">
        <h1 class="page-title">Record Expense</h1>
        <div class="page-actions">
          <a href="#/expenses" class="btn btn-secondary">Cancel</a>
          <button class="btn btn-primary" onclick="ExpensesModule.save()">
            <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M19 21H5a2 2 0 01-2-2V5a2 2 0 012-2h11l5 5v11a2 2 0 01-2 2z"/><polyline points="17 21 17 13 7 13 7 21"/></svg>
            Save & Post to Ledger
          </button>
        </div>
      </div>
      <div class="card" style="max-width:700px">
        <div class="form-section">
          <div class="form-section-title">Expense Details</div>
          <div class="form-row">
            <div class="form-group">
              <label class="form-label required">Category</label>
              <select class="form-control" id="exp-category">
                <option value="Office Supplies">Office Supplies</option>
                <option value="Rent">Rent</option>
                <option value="Utilities">Utilities</option>
                <option value="Travel">Travel</option>
                <option value="Salaries">Salaries & Wages</option>
                <option value="Advertising">Advertising</option>
                <option value="Internet">Telephone & Internet</option>
                <option value="Miscellaneous">Miscellaneous</option>
              </select>
            </div>
            <div class="form-group">
              <label class="form-label required">Amount (₹)</label>
              <input class="form-control" type="number" step="0.01" id="exp-amount" placeholder="0.00">
            </div>
          </div>
          <div class="form-row">
            <div class="form-group">
              <label class="form-label required">Date</label>
              <input class="form-control" type="date" id="exp-date" value="${new Date().toISOString().split('T')[0]}">
            </div>
            <div class="form-group">
              <label class="form-label">Paid Via</label>
              <select class="form-control" id="exp-paid-via">
                <option value="cash">Cash</option>
                <option value="bank">Bank Transfer</option>
                <option value="upi">UPI</option>
                <option value="card">Credit/Debit Card</option>
              </select>
            </div>
          </div>
          <div class="form-group">
            <label class="form-label">Notes / Reference</label>
            <textarea class="form-control" id="exp-notes" placeholder="Receipt number, vendor name, description..."></textarea>
          </div>
        </div>
        <div style="padding:12px;background:var(--primary-l);border-radius:var(--radius);font-size:.78rem;color:var(--text-s)">
          ℹ️ This expense will be automatically recorded as a balanced double-entry: Debit Expense → Credit Cash/Bank.
        </div>
      </div>`;
  },

  async save() {
    const amount = Number(document.getElementById('exp-amount')?.value);
    if (!amount || amount <= 0) { QManage.toast('Enter a valid amount', 'error'); return; }

    const category = document.getElementById('exp-category')?.value || 'General';
    const dateStr = document.getElementById('exp-date')?.value;
    const notes = document.getElementById('exp-notes')?.value || '';
    const dateTs = Math.floor(new Date(dateStr).getTime() / 1000);

    try {
      await API.post('/expenses', {
        category: category,
        amount: amount,
        date: dateTs,
        notes: notes
      });
      QManage.toast(`Expense ₹${amount.toLocaleString('en-IN', {minimumFractionDigits:2})} posted to ledger`, 'success');
      location.hash = '#/expenses';
    } catch(e) {
      QManage.toast('Save failed: ' + e.message, 'error');
    }
  },

  async renderForm() { this.showCreateForm(); return ''; }
};
