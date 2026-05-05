/* Q Manage — Bills (Vendor Invoices) Module */
const BillsModule = {
  async renderList() {
    let bills = [];
    try { bills = await API.get('/bills'); } catch(e) {}

    const fmt = (n) => '₹' + Number(n || 0).toLocaleString('en-IN', { minimumFractionDigits: 2 });
    const dt = (ts) => ts ? new Date(ts * 1000).toLocaleDateString('en-IN', { day: '2-digit', month: 'short', year: 'numeric' }) : '-';

    const unpaid = bills.filter(b => b.status !== 'paid' && b.status !== 'void');
    const totalPayable = unpaid.reduce((s, b) => s + (b.balance_due || 0), 0);

    return `
      <div class="page-header">
        <h1 class="page-title">Bills</h1>
        <div class="page-actions">
          <button class="btn btn-primary" onclick="BillsModule.showCreateForm()">
            <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/></svg>
            New Bill
          </button>
        </div>
      </div>
      <div class="stats-grid">
        <div class="stat-card blue"><span class="stat-label">Total Bills</span><span class="stat-value">${bills.length}</span></div>
        <div class="stat-card red"><span class="stat-label">Total Payable</span><span class="stat-value">${fmt(totalPayable)}</span></div>
        <div class="stat-card yellow"><span class="stat-label">Unpaid</span><span class="stat-value">${unpaid.length}</span></div>
      </div>
      <div class="table-container">
        ${bills.length ? `
        <table class="data-table">
          <thead><tr><th>Date</th><th>Bill #</th><th>Vendor</th><th>Status</th><th style="text-align:right">Total</th><th style="text-align:right">Balance</th></tr></thead>
          <tbody>
            ${bills.map(b => `
              <tr>
                <td>${dt(b.date)}</td>
                <td style="font-weight:600;color:var(--primary)">${b.bill_num}</td>
                <td>${b.vendor_name || '-'}</td>
                <td><span class="badge badge-${b.status === 'paid' ? 'paid' : b.status === 'partial' ? 'partial' : 'sent'}">${(b.status||'draft').toUpperCase()}</span></td>
                <td style="text-align:right;font-weight:500">${fmt(b.total)}</td>
                <td style="text-align:right;font-weight:600;color:${b.balance_due > 0 ? 'var(--red)' : 'var(--green)'}">${fmt(b.balance_due)}</td>
              </tr>
            `).join('')}
          </tbody>
        </table>` : `
        <div class="empty-state">
          <h3>No bills yet</h3>
          <p>Record vendor invoices and track your payables.</p>
          <button onclick="BillsModule.showCreateForm()" class="btn btn-primary">+ New Bill</button>
        </div>`}
      </div>`;
  },

  async showCreateForm() {
    let contacts = [];
    try { contacts = (await API.get('/contacts')).filter(c => c.type === 'vendor' || c.type === 'both'); } catch(e) {}

    document.getElementById('page-content').innerHTML = `
      <div class="page-header">
        <h1 class="page-title">New Bill</h1>
        <div class="page-actions">
          <a href="#/bills" class="btn btn-secondary">Cancel</a>
          <button class="btn btn-primary" onclick="BillsModule.save()">Save & Post to Ledger</button>
        </div>
      </div>
      <div class="card" style="max-width:700px">
        <div class="form-section">
          <div class="form-row">
            <div class="form-group">
              <label class="form-label required">Bill Number</label>
              <input class="form-control" id="bill-num" value="BILL-${String(Date.now()).slice(-5)}">
            </div>
            <div class="form-group">
              <label class="form-label required">Date</label>
              <input class="form-control" type="date" id="bill-date" value="${new Date().toISOString().split('T')[0]}">
            </div>
          </div>
          <div class="form-row">
            <div class="form-group">
              <label class="form-label required">Vendor</label>
              <select class="form-control" id="bill-vendor">
                <option value="">Select Vendor</option>
                ${contacts.map(c => `<option value="${c.id}">${c.display_name}</option>`).join('')}
              </select>
            </div>
            <div class="form-group">
              <label class="form-label required">Amount (₹)</label>
              <input class="form-control" type="number" step="0.01" id="bill-amount" placeholder="0.00">
            </div>
          </div>
          <div class="form-group">
            <label class="form-label">Notes</label>
            <textarea class="form-control" id="bill-notes" placeholder="Bill details..."></textarea>
          </div>
        </div>
        <div style="padding:12px;background:var(--primary-l);border-radius:var(--radius);font-size:.78rem;color:var(--text-s)">
          ℹ️ Bill auto-posts to ledger: Debit Expense → Credit Accounts Payable
        </div>
      </div>`;
  },

  async renderForm() {
    await this.showCreateForm();
    return '';
  },

  async save() {
    const num = document.getElementById('bill-num')?.value?.trim();
    const vendor = document.getElementById('bill-vendor')?.value;
    const amount = Number(document.getElementById('bill-amount')?.value) ;

    if (!num || !vendor || amount <= 0) { QManage.toast('Fill all required fields', 'error'); return; }

    try {
      await API.post('/bills', {
        bill_num: num,
        vendor_id: parseInt(vendor),
        total: amount,
        date: Math.floor(new Date(document.getElementById('bill-date')?.value).getTime() / 1000),
        notes: document.getElementById('bill-notes')?.value || ''
      });
      QManage.toast(`Bill ${num} posted to ledger`, 'success');
      location.hash = '#/bills';
    } catch(e) { QManage.toast('Failed: ' + e.message, 'error'); }
  }
};
