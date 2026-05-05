/* Q Manage — Credit Notes Module (Sales Returns / Adjustments) */
const CreditNotesModule = {
  async renderList() {
    let notes = [], contacts = [], invoices = [];
    try { notes = await API.get('/credit-notes'); } catch(e) {}
    try { contacts = await API.get('/contacts'); } catch(e) {}
    try { invoices = await API.get('/invoices'); } catch(e) {}

    const fmt = (n) => '₹' + Number(n || 0).toLocaleString('en-IN', { minimumFractionDigits: 2 });
    const dt = (ts) => ts ? new Date(ts * 1000).toLocaleDateString('en-IN', { day: '2-digit', month: 'short', year: 'numeric' }) : '-';
    const totalCN = notes.reduce((s, n) => s + (n.amount || 0), 0);

    return `
      <div class="page-header">
        <h1 class="page-title">Credit Notes</h1>
        <div class="page-actions">
          <button class="btn btn-primary" onclick="CreditNotesModule.showCreateForm()">
            <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/></svg>
            New Credit Note
          </button>
        </div>
      </div>
      <div class="stats-grid">
        <div class="stat-card blue"><span class="stat-label">Total Credit Notes</span><span class="stat-value">${notes.length}</span></div>
        <div class="stat-card red"><span class="stat-label">Total Value</span><span class="stat-value">${fmt(totalCN)}</span></div>
      </div>
      <div class="table-container">
        ${notes.length ? `
        <table class="data-table">
          <thead><tr><th>Date</th><th>CN #</th><th>Customer</th><th>Invoice</th><th>Reason</th><th style="text-align:right">Amount</th></tr></thead>
          <tbody>
            ${notes.map(n => `
              <tr>
                <td>${dt(n.date)}</td>
                <td style="font-weight:600;color:var(--primary)">${n.cn_num || '-'}</td>
                <td>${n.customer_name || '-'}</td>
                <td style="font-family:monospace;font-size:.75rem">${n.invoice_id || '-'}</td>
                <td style="font-size:.82rem">${n.reason || '-'}</td>
                <td style="text-align:right;font-weight:600;color:var(--red)">${fmt(n.amount)}</td>
              </tr>
            `).join('')}
          </tbody>
        </table>` : `
        <div class="empty-state">
          <h3>No credit notes yet</h3>
          <p>Issue credit notes for returns, adjustments, or overbilling corrections. Each CN auto-posts a reversal to the ledger.</p>
          <button onclick="CreditNotesModule.showCreateForm()" class="btn btn-primary">+ Issue Credit Note</button>
        </div>`}
      </div>`;
  },

  async showCreateForm() {
    let contacts = [], invoices = [];
    try { contacts = (await API.get('/contacts')).filter(c => c.type === 'customer' || c.type === 'both'); } catch(e) {}
    try { invoices = (await API.get('/invoices')).filter(i => i.status !== 'void'); } catch(e) {}

    document.getElementById('page-content').innerHTML = `
      <div class="page-header">
        <h1 class="page-title">Issue Credit Note</h1>
        <div class="page-actions">
          <a href="#/credit-notes" class="btn btn-secondary">Cancel</a>
          <button class="btn btn-primary" onclick="CreditNotesModule.save()">Issue & Post to Ledger</button>
        </div>
      </div>
      <div class="card" style="max-width:700px">
        <div class="form-section">
          <div class="form-section-title">Credit Note Details</div>
          <div class="form-row">
            <div class="form-group">
              <label class="form-label required">Customer</label>
              <select class="form-control" id="cn-customer">
                <option value="">Select Customer</option>
                ${contacts.map(c => `<option value="${c.id}">${c.display_name}</option>`).join('')}
              </select>
            </div>
            <div class="form-group">
              <label class="form-label">Against Invoice</label>
              <select class="form-control" id="cn-invoice">
                <option value="">None (Standalone)</option>
                ${invoices.map(i => `<option value="${i.id}">${i.invoice_num} — ₹${(i.total/100).toLocaleString('en-IN')}</option>`).join('')}
              </select>
            </div>
          </div>
          <div class="form-row">
            <div class="form-group">
              <label class="form-label required">Amount (₹)</label>
              <input class="form-control" type="number" step="0.01" id="cn-amount" placeholder="0.00">
            </div>
            <div class="form-group">
              <label class="form-label required">Reason</label>
              <select class="form-control" id="cn-reason">
                <option value="Return">Goods Returned</option>
                <option value="Overbilling">Overbilling Correction</option>
                <option value="Defective">Defective Goods</option>
                <option value="Discount">Post-Sale Discount</option>
                <option value="Other">Other</option>
              </select>
            </div>
          </div>
          <div class="form-group">
            <label class="form-label">Additional Notes</label>
            <textarea class="form-control" id="cn-notes" placeholder="Details about the credit note..."></textarea>
          </div>
        </div>
        <div style="padding:12px;background:var(--primary-l);border-radius:var(--radius);font-size:.78rem;color:var(--text-s)">
          ℹ️ Credit Note auto-posts reversal to ledger: Debit Sales Revenue → Credit Accounts Receivable
        </div>
      </div>`;
  },

  async save() {
    const customer = document.getElementById('cn-customer')?.value;
    const amount = Number(document.getElementById('cn-amount')?.value) ;
    if (!customer || amount <= 0) { QManage.toast('Customer and amount required', 'error'); return; }

    try {
      const res = await API.post('/credit-notes', {
        customer_id: parseInt(customer),
        invoice_id: parseInt(document.getElementById('cn-invoice')?.value) || 0,
        amount,
        reason: document.getElementById('cn-reason')?.value + (document.getElementById('cn-notes')?.value ? ': ' + document.getElementById('cn-notes').value : '')
      });
      QManage.toast(`Credit Note ${res.credit_note_num} issued — reversal posted to ledger`, 'success');
      location.hash = '#/credit-notes';
    } catch(e) { QManage.toast('Failed: ' + e.message, 'error'); }
  }
};
