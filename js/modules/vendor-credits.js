/* Q Manage — Vendor Credits (Debit Notes) Module */
const VendorCreditsModule = {
  async renderList() {
    let credits = [];
    try { credits = await API.get('/vendor-credits'); } catch(e) {}

    const fmt = (n) => '₹' + Number(n || 0).toLocaleString('en-IN', { minimumFractionDigits: 2 });
    const dt = (ts) => ts ? new Date(ts * 1000).toLocaleDateString('en-IN', { day: '2-digit', month: 'short', year: 'numeric' }) : '-';
    const totalVC = credits.reduce((s, v) => s + (v.amount || 0), 0);

    return `
      <div class="page-header">
        <h1 class="page-title">Vendor Credits</h1>
        <div class="page-actions">
          <button class="btn btn-primary" onclick="VendorCreditsModule.showCreateForm()">
            <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/></svg>
            New Vendor Credit
          </button>
        </div>
      </div>
      <div class="stats-grid">
        <div class="stat-card blue"><span class="stat-label">Total Vendor Credits</span><span class="stat-value">${credits.length}</span></div>
        <div class="stat-card green"><span class="stat-label">Total Value</span><span class="stat-value">${fmt(totalVC)}</span></div>
      </div>
      <div class="table-container">
        ${credits.length ? `
        <table class="data-table">
          <thead><tr><th>Date</th><th>VC #</th><th>Vendor</th><th>Bill</th><th>Reason</th><th style="text-align:right">Amount</th></tr></thead>
          <tbody>
            ${credits.map(v => `
              <tr>
                <td>${dt(v.date)}</td>
                <td style="font-weight:600;color:var(--primary)">${v.vc_num || '-'}</td>
                <td>${v.vendor_name || '-'}</td>
                <td style="font-family:monospace;font-size:.75rem">${v.bill_id || '-'}</td>
                <td style="font-size:.82rem">${v.reason || '-'}</td>
                <td style="text-align:right;font-weight:600;color:var(--green)">${fmt(v.amount)}</td>
              </tr>
            `).join('')}
          </tbody>
        </table>` : `
        <div class="empty-state">
          <h3>No vendor credits yet</h3>
          <p>Record debit notes from vendors for returns, defects, or pricing adjustments.</p>
          <button onclick="VendorCreditsModule.showCreateForm()" class="btn btn-primary">+ Record Vendor Credit</button>
        </div>`}
      </div>`;
  },

  async showCreateForm() {
    let contacts = [], bills = [];
    try { contacts = (await API.get('/contacts')).filter(c => c.type === 'vendor' || c.type === 'both'); } catch(e) {}
    try { bills = (await API.get('/bills')).filter(b => b.status !== 'void'); } catch(e) {}

    document.getElementById('page-content').innerHTML = `
      <div class="page-header">
        <h1 class="page-title">Record Vendor Credit</h1>
        <div class="page-actions">
          <a href="#/vendor-credits" class="btn btn-secondary">Cancel</a>
          <button class="btn btn-primary" onclick="VendorCreditsModule.save()">Record & Post to Ledger</button>
        </div>
      </div>
      <div class="card" style="max-width:700px">
        <div class="form-section">
          <div class="form-row">
            <div class="form-group">
              <label class="form-label required">Vendor</label>
              <select class="form-control" id="vc-vendor">
                <option value="">Select Vendor</option>
                ${contacts.map(c => `<option value="${c.id}">${c.display_name}</option>`).join('')}
              </select>
            </div>
            <div class="form-group">
              <label class="form-label">Against Bill</label>
              <select class="form-control" id="vc-bill">
                <option value="">None (Standalone)</option>
                ${bills.map(b => `<option value="${b.id}">${b.bill_num} — ₹${(b.total/100).toLocaleString('en-IN')}</option>`).join('')}
              </select>
            </div>
          </div>
          <div class="form-row">
            <div class="form-group">
              <label class="form-label required">Amount (₹)</label>
              <input class="form-control" type="number" step="0.01" id="vc-amount" placeholder="0.00">
            </div>
            <div class="form-group">
              <label class="form-label required">Reason</label>
              <select class="form-control" id="vc-reason">
                <option value="Return">Goods Returned to Vendor</option>
                <option value="Defective">Defective/Damaged Goods</option>
                <option value="Price Adjustment">Price Adjustment</option>
                <option value="Other">Other</option>
              </select>
            </div>
          </div>
        </div>
        <div style="padding:12px;background:var(--green-l);border-radius:var(--radius);font-size:.78rem;color:var(--text-s)">
          ✅ Vendor Credit auto-posts: Debit Accounts Payable → Credit Expense (reduces your payable)
        </div>
      </div>`;
  },

  async save() {
    const vendor = document.getElementById('vc-vendor')?.value;
    const amount = Number(document.getElementById('vc-amount')?.value) ;
    if (!vendor || amount <= 0) { QManage.toast('Vendor and amount required', 'error'); return; }

    try {
      const res = await API.post('/vendor-credits', {
        vendor_id: parseInt(vendor),
        bill_id: parseInt(document.getElementById('vc-bill')?.value) || 0,
        amount,
        reason: document.getElementById('vc-reason')?.value || ''
      });
      QManage.toast(`Vendor Credit ${res.vendor_credit_num} recorded`, 'success');
      location.hash = '#/vendor-credits';
    } catch(e) { QManage.toast('Failed: ' + e.message, 'error'); }
  }
};
