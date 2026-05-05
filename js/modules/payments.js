/* Q Manage — Payments Module (Received + Made) */
const PaymentsModule = {
  async renderList() {
    let received = [], made = [];
    try { received = await API.get('/payments/received'); } catch(e) {}
    try { made = await API.get('/payments/made'); } catch(e) {}

    const fmt = (n) => '₹' + Number(n || 0).toLocaleString('en-IN', { minimumFractionDigits: 2 });
    const dt = (ts) => ts ? new Date(ts * 1000).toLocaleDateString('en-IN', { day: '2-digit', month: 'short', year: 'numeric' }) : '-';

    const totalIn = received.reduce((s, p) => s + (p.amount || 0), 0);
    const totalOut = made.reduce((s, p) => s + (p.amount || 0), 0);

    return `
      <div class="page-header">
        <h1 class="page-title">Payments</h1>
        <div class="page-actions">
          <button class="btn btn-secondary" onclick="PaymentsModule.showPaymentMade()">Record Payment Made</button>
          <button class="btn btn-primary" onclick="PaymentsModule.showPaymentReceived()">Record Payment Received</button>
        </div>
      </div>

      <div class="stats-grid">
        <div class="stat-card green">
          <span class="stat-label">RECEIVED</span>
          <span class="stat-value">${fmt(totalIn)}</span>
          <span class="stat-change up">${received.length} payments</span>
        </div>
        <div class="stat-card red">
          <span class="stat-label">MADE</span>
          <span class="stat-value">${fmt(totalOut)}</span>
          <span class="stat-change">${made.length} payments</span>
        </div>
        <div class="stat-card blue">
          <span class="stat-label">NET FLOW</span>
          <span class="stat-value" style="color:${totalIn - totalOut >= 0 ? 'var(--green)' : 'var(--red)'}">${fmt(totalIn - totalOut)}</span>
        </div>
      </div>

      <div class="tabs" id="payment-tabs">
        <div class="tab active" onclick="PaymentsModule.switchTab('received')">Received (${received.length})</div>
        <div class="tab" onclick="PaymentsModule.switchTab('made')">Made (${made.length})</div>
      </div>

      <div id="tab-received" class="table-container">
        ${received.length ? `
        <table class="data-table">
          <thead><tr><th>Date</th><th>Payment #</th><th>Customer</th><th>Mode</th><th>Reference</th><th style="text-align:right">Amount</th></tr></thead>
          <tbody>
            ${received.map(p => `
              <tr>
                <td>${dt(p.date)}</td>
                <td style="font-weight:600;color:var(--primary)">${p.payment_num || '-'}</td>
                <td>${p.customer_name || '-'}</td>
                <td><span class="badge badge-paid">${(p.mode || 'bank').toUpperCase()}</span></td>
                <td style="font-family:monospace;font-size:.72rem">${p.reference || '-'}</td>
                <td style="text-align:right;font-weight:600;color:var(--green)">${fmt(p.amount)}</td>
              </tr>
            `).join('')}
          </tbody>
        </table>` : '<div class="empty-state" style="padding:40px"><p>No payments received yet.</p></div>'}
      </div>
      <div id="tab-made" class="table-container" style="display:none">
        ${made.length ? `
        <table class="data-table">
          <thead><tr><th>Date</th><th>Payment #</th><th>Vendor</th><th>Mode</th><th style="text-align:right">Amount</th></tr></thead>
          <tbody>
            ${made.map(p => `
              <tr>
                <td>${dt(p.date)}</td>
                <td style="font-weight:600">${p.payment_num || '-'}</td>
                <td>${p.vendor_name || '-'}</td>
                <td><span class="badge badge-sent">${(p.mode || 'bank').toUpperCase()}</span></td>
                <td style="text-align:right;font-weight:600;color:var(--red)">${fmt(p.amount)}</td>
              </tr>
            `).join('')}
          </tbody>
        </table>` : '<div class="empty-state" style="padding:40px"><p>No payments made yet.</p></div>'}
      </div>`;
  },

  switchTab(tab) {
    document.getElementById('tab-received').style.display = tab === 'received' ? '' : 'none';
    document.getElementById('tab-made').style.display = tab === 'made' ? '' : 'none';
    document.querySelectorAll('#payment-tabs .tab').forEach((t, i) => {
      t.classList.toggle('active', (i === 0 && tab === 'received') || (i === 1 && tab === 'made'));
    });
  },

  async showPaymentReceived() {
    let contacts = [], invoices = [];
    try { contacts = (await API.get('/contacts')).filter(c => c.type === 'customer' || c.type === 'both'); } catch(e) {}
    try { invoices = (await API.get('/invoices')).filter(i => i.status !== 'paid' && i.status !== 'void'); } catch(e) {}

    document.getElementById('page-content').innerHTML = `
      <div class="page-header">
        <h1 class="page-title">Record Payment Received</h1>
        <div class="page-actions">
          <a href="#/payments" class="btn btn-secondary">Cancel</a>
          <button class="btn btn-primary" onclick="PaymentsModule.saveReceived()">Record Payment</button>
        </div>
      </div>
      <div class="card" style="max-width:700px">
        <div class="form-section">
          <div class="form-row">
            <div class="form-group">
              <label class="form-label required">Customer</label>
              <select class="form-control" id="pr-customer">
                <option value="">Select</option>
                ${contacts.map(c => `<option value="${c.id}">${c.display_name}</option>`).join('')}
              </select>
            </div>
            <div class="form-group">
              <label class="form-label">Against Invoice</label>
              <select class="form-control" id="pr-invoice">
                <option value="">Select Invoice (optional)</option>
                ${invoices.map(i => `<option value="${i.id}">${i.invoice_num} — ₹${(i.balance_due/100).toLocaleString('en-IN')}</option>`).join('')}
              </select>
            </div>
          </div>
          <div class="form-row">
            <div class="form-group">
              <label class="form-label required">Amount (₹)</label>
              <input class="form-control" type="number" step="0.01" id="pr-amount" placeholder="0.00">
            </div>
            <div class="form-group">
              <label class="form-label">Payment Mode</label>
              <select class="form-control" id="pr-mode">
                <option value="bank_transfer">Bank Transfer</option>
                <option value="upi">UPI</option>
                <option value="cash">Cash</option>
                <option value="cheque">Cheque</option>
                <option value="card">Card</option>
              </select>
            </div>
          </div>
          <div class="form-group">
            <label class="form-label">UTR / Reference Number</label>
            <input class="form-control" id="pr-ref" placeholder="UTR or cheque number">
          </div>
        </div>
        <div style="padding:12px;background:var(--green-l);border-radius:var(--radius);font-size:.78rem;color:var(--text-s)">
          ✅ Payment auto-posts: Debit Bank → Credit Accounts Receivable. Invoice status auto-updates.
        </div>
      </div>`;
  },

  async saveReceived() {
    const customer = document.getElementById('pr-customer')?.value;
    const amount = Number(document.getElementById('pr-amount')?.value) ;
    if (!customer || amount <= 0) { QManage.toast('Customer and amount required', 'error'); return; }

    try {
      await API.post('/payments/received', {
        customer_id: parseInt(customer),
        invoice_id: parseInt(document.getElementById('pr-invoice')?.value) || 0,
        amount, mode: document.getElementById('pr-mode')?.value || 'bank_transfer',
        reference: document.getElementById('pr-ref')?.value || '',
        date: Math.floor(Date.now() / 1000)
      });
      QManage.toast('Payment recorded & ledger updated', 'success');
      location.hash = '#/payments';
    } catch(e) { QManage.toast('Failed: ' + e.message, 'error'); }
  },

  async showPaymentMade() {
    let contacts = [];
    try { contacts = (await API.get('/contacts')).filter(c => c.type === 'vendor' || c.type === 'both'); } catch(e) {}

    document.getElementById('page-content').innerHTML = `
      <div class="page-header">
        <h1 class="page-title">Record Payment Made</h1>
        <div class="page-actions">
          <a href="#/payments" class="btn btn-secondary">Cancel</a>
          <button class="btn btn-primary" onclick="PaymentsModule.saveMade()">Record Payment</button>
        </div>
      </div>
      <div class="card" style="max-width:700px">
        <div class="form-section">
          <div class="form-row">
            <div class="form-group">
              <label class="form-label required">Vendor</label>
              <select class="form-control" id="pm-vendor">
                <option value="">Select</option>
                ${contacts.map(c => `<option value="${c.id}">${c.display_name}</option>`).join('')}
              </select>
            </div>
            <div class="form-group">
              <label class="form-label required">Amount (₹)</label>
              <input class="form-control" type="number" step="0.01" id="pm-amount" placeholder="0.00">
            </div>
          </div>
          <div class="form-group">
            <label class="form-label">Payment Mode</label>
            <select class="form-control" id="pm-mode">
              <option value="bank_transfer">Bank Transfer</option>
              <option value="upi">UPI</option>
              <option value="cash">Cash</option>
              <option value="cheque">Cheque</option>
            </select>
          </div>
        </div>
      </div>`;
  },

  async saveMade() {
    const vendor = document.getElementById('pm-vendor')?.value;
    const amount = Number(document.getElementById('pm-amount')?.value) ;
    if (!vendor || amount <= 0) { QManage.toast('Vendor and amount required', 'error'); return; }

    try {
      await API.post('/payments/made', {
        vendor_id: parseInt(vendor), amount,
        mode: document.getElementById('pm-mode')?.value || 'bank_transfer',
        date: Math.floor(Date.now() / 1000)
      });
      QManage.toast('Payment recorded', 'success');
      location.hash = '#/payments';
    } catch(e) { QManage.toast('Failed: ' + e.message, 'error'); }
  }
};
