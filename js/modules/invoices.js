/* Q Manage — Invoices Module (C API Integrated) */
const InvoicesModule = {
  async renderList() {
    let invoices = [];
    try { invoices = await API.get('/invoices'); } catch(e) { /* offline */ }
    
    const fmt = (n) => '₹' + Number(n || 0).toLocaleString('en-IN', { minimumFractionDigits: 2 });
    const dt = (ts) => ts ? new Date(ts * 1000).toLocaleDateString('en-IN', { day: '2-digit', month: 'short', year: 'numeric' }) : '-';

    return `
      <div class="page-header">
        <h1 class="page-title">Invoices</h1>
        <div class="page-actions">
          <button class="btn btn-primary" onclick="InvoicesModule.showCreateForm()">
            <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/></svg>
            New Invoice
          </button>
        </div>
      </div>
      <div class="table-container">
        <div class="table-toolbar">
          <div class="table-filters">
            <button class="filter-btn active">All (${invoices.length})</button>
            <button class="filter-btn">Draft</button>
            <button class="filter-btn">Sent</button>
            <button class="filter-btn">Paid</button>
          </div>
        </div>
        ${invoices.length ? `
        <table class="data-table">
          <thead><tr>
            <th>Date</th><th>Invoice#</th><th>Customer</th><th>Status</th><th style="text-align:right">Amount</th>
          </tr></thead>
          <tbody>
            ${invoices.map(inv => `
              <tr style="cursor:pointer">
                <td>${dt(inv.date)}</td>
                <td style="font-weight:600;color:var(--primary)">${inv.invoice_num}</td>
                <td>${inv.customer_name || '-'}</td>
                <td><span class="badge badge-${inv.status || 'draft'}">${(inv.status || 'draft').toUpperCase()}</span></td>
                <td style="text-align:right;font-weight:600">${fmt(inv.total)}</td>
              </tr>
            `).join('')}
          </tbody>
        </table>` : `
        <div class="empty-state">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" style="width:64px;height:64px"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><polyline points="14 2 14 8 20 8"/></svg>
          <h3>No invoices yet</h3>
          <p>Create your first invoice to start tracking revenue through the ledger.</p>
          <button onclick="InvoicesModule.showCreateForm()" class="btn btn-primary">+ Create Invoice</button>
        </div>`}
      </div>`;
  },

  async showCreateForm() {
    let contacts = [];
    try { contacts = await API.get('/contacts'); } catch(e) {}

    const content = document.getElementById('page-content');
    content.innerHTML = `
      <div class="page-header">
        <h1 class="page-title">New Invoice</h1>
        <div class="page-actions">
          <a href="#/invoices" class="btn btn-secondary">Cancel</a>
          <button class="btn btn-primary" onclick="InvoicesModule.save()">
            <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M19 21H5a2 2 0 01-2-2V5a2 2 0 012-2h11l5 5v11a2 2 0 01-2 2z"/><polyline points="17 21 17 13 7 13 7 21"/></svg>
            Save & Post to Ledger
          </button>
        </div>
      </div>
      <div class="card" style="max-width:800px">
        <div class="form-section">
          <div class="form-section-title">Invoice Details</div>
          <div class="form-row">
            <div class="form-group">
              <label class="form-label required">Invoice Number</label>
              <input class="form-control" id="inv-num" value="INV-${String(Date.now()).slice(-5)}" placeholder="INV-00001">
            </div>
            <div class="form-group">
              <label class="form-label required">Date</label>
              <input class="form-control" type="date" id="inv-date" value="${new Date().toISOString().split('T')[0]}">
            </div>
          </div>
          <div class="form-group">
            <label class="form-label required">Customer</label>
            <select class="form-control" id="inv-customer">
              <option value="">Select Customer</option>
              ${contacts.map(c => `<option value="${c.id}">${c.display_name}</option>`).join('')}
            </select>
            <div style="font-size:.72rem;color:var(--text-xs);margin-top:4px">
              No customers? <a href="#/contacts" style="color:var(--primary)">Add one first</a>
            </div>
          </div>
        </div>

        <div class="form-section">
          <div class="form-section-title">Line Items</div>
          <table class="line-items" id="inv-lines-table">
            <thead><tr><th>Description</th><th style="width:100px">Qty</th><th style="width:120px">Rate (₹)</th><th style="width:120px">Amount</th><th style="width:40px"></th></tr></thead>
            <tbody id="inv-lines">
              <tr>
                <td><input placeholder="Item description" class="line-desc"></td>
                <td><input type="number" value="1" min="1" class="line-qty" onchange="InvoicesModule.calcTotals()"></td>
                <td><input type="number" step="0.01" value="0" class="line-rate" onchange="InvoicesModule.calcTotals()"></td>
                <td class="line-amount" style="text-align:right;font-weight:500;padding:6px 12px">₹0.00</td>
                <td><button class="remove-line" onclick="this.closest('tr').remove();InvoicesModule.calcTotals()" title="Remove">×</button></td>
              </tr>
            </tbody>
          </table>
          <button class="add-line-btn" onclick="InvoicesModule.addLine()">+ Add Line Item</button>
        </div>

        <div class="invoice-totals">
          <table class="totals-table">
            <tr><td>Subtotal</td><td id="inv-subtotal">₹0.00</td></tr>
            <tr><td>Tax (18% GST)</td><td id="inv-tax">₹0.00</td></tr>
            <tr class="grand-total"><td>Total</td><td id="inv-total">₹0.00</td></tr>
          </table>
        </div>

        <div class="form-section" style="margin-top:20px">
          <div class="form-group">
            <label class="form-label">Notes</label>
            <textarea class="form-control" id="inv-notes" placeholder="Payment terms, thank you message, etc."></textarea>
          </div>
        </div>
      </div>`;
  },

  addLine() {
    const tbody = document.getElementById('inv-lines');
    const tr = document.createElement('tr');
    tr.innerHTML = `
      <td><input placeholder="Item description" class="line-desc"></td>
      <td><input type="number" value="1" min="1" class="line-qty" onchange="InvoicesModule.calcTotals()"></td>
      <td><input type="number" step="0.01" value="0" class="line-rate" onchange="InvoicesModule.calcTotals()"></td>
      <td class="line-amount" style="text-align:right;font-weight:500;padding:6px 12px">₹0.00</td>
      <td><button class="remove-line" onclick="this.closest('tr').remove();InvoicesModule.calcTotals()" title="Remove">×</button></td>`;
    tbody.appendChild(tr);
  },

  calcTotals() {
    const fmt = (n) => '₹' + Number(n).toLocaleString('en-IN', { minimumFractionDigits: 2 });
    const rows = document.querySelectorAll('#inv-lines tr');
    let subtotal = 0;
    rows.forEach(row => {
      const qty = Number(row.querySelector('.line-qty')?.value) || 0;
      const rate = Number(row.querySelector('.line-rate')?.value) || 0;
      const amt = qty * rate;
      subtotal += amt;
      const amtCell = row.querySelector('.line-amount');
      if (amtCell) amtCell.textContent = fmt(amt);
    });
    const tax = subtotal * 0.18;
    const total = subtotal + tax;
    document.getElementById('inv-subtotal').textContent = fmt(subtotal);
    document.getElementById('inv-tax').textContent = fmt(tax);
    document.getElementById('inv-total').textContent = fmt(total);
  },

  async save() {
    const num = document.getElementById('inv-num')?.value?.trim();
    const dateStr = document.getElementById('inv-date')?.value;
    const custId = document.getElementById('inv-customer')?.value;

    if (!num) { QManage.toast('Invoice number is required', 'error'); return; }
    if (!custId) { QManage.toast('Please select a customer', 'error'); return; }

    // Calculate total from line items
    const rows = document.querySelectorAll('#inv-lines tr');
    let subtotal = 0;
    rows.forEach(row => {
      const qty = Number(row.querySelector('.line-qty')?.value) || 0;
      const rate = Number(row.querySelector('.line-rate')?.value) || 0;
      subtotal += qty * rate;
    });
    const total = subtotal + (subtotal * 0.18); // 18% GST

    if (total <= 0) { QManage.toast('Invoice total must be greater than zero', 'error'); return; }

    const dateTs = Math.floor(new Date(dateStr).getTime() / 1000);

    try {
      await API.post('/invoices', {
        invoice_num: num,
        customer_id: parseInt(custId),
        total: total,
        date: dateTs
      });
      QManage.toast(`Invoice ${num} created — ₹${total.toLocaleString('en-IN', {minimumFractionDigits:2})} posted to ledger`, 'success');
      location.hash = '#/invoices';
    } catch (e) {
      QManage.toast('Save failed: ' + e.message, 'error');
    }
  },

  // Keep legacy methods for backward compatibility
  async renderForm() { await this.showCreateForm(); return ''; },

  async renderView(id) {
    let invoices = [];
    let payments = [];
    try { invoices = await API.get('/invoices'); } catch(e) {}
    try { payments = await API.get('/payments/received'); } catch(e) {}

    const inv = invoices.find(i => i.id == id);
    if (!inv) {
      return `<div class="empty-state"><h3>Invoice not found</h3><a href="#/invoices" class="btn btn-secondary">Back</a></div>`;
    }

    const fmt = (n) => '₹' + Number(n || 0).toLocaleString('en-IN', { minimumFractionDigits: 2 });
    const dt = (ts) => ts ? new Date(ts * 1000).toLocaleDateString('en-IN', { day: '2-digit', month: 'short', year: 'numeric' }) : '-';

    // Get payments against this invoice
    const invPayments = payments.filter(p => p.invoice_id == id);
    const totalPaid = invPayments.reduce((s, p) => s + (p.amount || 0), 0);
    const balanceDue = (inv.total || 0) - totalPaid;
    const isPaid = balanceDue <= 0 || inv.status === 'paid';

    return `
      <div class="page-header">
        <h1 class="page-title">${inv.invoice_num}</h1>
        <div class="page-actions">
          <a href="#/invoices" class="btn btn-secondary">Back</a>
          <button class="btn btn-secondary" onclick="InvoicesModule.downloadPDF(${inv.id})">Download PDF</button>
          ${!isPaid ? `<button class="btn btn-primary" onclick="InvoicesModule.showRecordPayment(${inv.id}, ${balanceDue})">Record Payment</button>` : ''}
        </div>
      </div>

      <div class="stats-grid">
        <div class="stat-card blue">
          <span class="stat-label">TOTAL</span>
          <span class="stat-value">${fmt(inv.total)}</span>
        </div>
        <div class="stat-card green">
          <span class="stat-label">PAID</span>
          <span class="stat-value">${fmt(totalPaid)}</span>
        </div>
        <div class="stat-card ${isPaid ? 'green' : 'red'}">
          <span class="stat-label">BALANCE DUE</span>
          <span class="stat-value">${fmt(balanceDue)}</span>
        </div>
        <div class="stat-card yellow">
          <span class="stat-label">STATUS</span>
          <span class="stat-value" style="font-size:1.1rem"><span class="badge badge-${inv.status || 'draft'}" style="font-size:.85rem;padding:6px 14px">${(inv.status || 'draft').toUpperCase()}</span></span>
        </div>
      </div>

      <div class="charts-grid">
        <div class="card">
          <div class="card-header"><span class="card-title">Invoice Details</span></div>
          <div style="padding:8px 0">
            <div style="display:grid;grid-template-columns:140px 1fr;gap:8px;font-size:.85rem">
              <span style="font-weight:600;color:var(--text-s)">Customer</span>
              <span style="font-weight:600">${inv.customer_name || '-'}</span>
              <span style="font-weight:600;color:var(--text-s)">Invoice Date</span>
              <span>${dt(inv.date)}</span>
              <span style="font-weight:600;color:var(--text-s)">Due Date</span>
              <span>${dt(inv.due_date) || 'On receipt'}</span>
              <span style="font-weight:600;color:var(--text-s)">Subtotal</span>
              <span>${fmt(inv.subtotal || inv.total)}</span>
              <span style="font-weight:600;color:var(--text-s)">Tax</span>
              <span>${fmt(inv.tax || 0)}</span>
              <span style="font-weight:600;color:var(--text-s)">Total</span>
              <span style="font-weight:700;font-size:1.05rem">${fmt(inv.total)}</span>
              ${inv.notes ? `<span style="font-weight:600;color:var(--text-s)">Notes</span><span>${inv.notes}</span>` : ''}
            </div>
          </div>
        </div>

        <div class="card">
          <div class="card-header"><span class="card-title">Payments (${invPayments.length})</span></div>
          ${invPayments.length ? `
          <table class="data-table">
            <thead><tr><th>Date</th><th>Mode</th><th>Ref</th><th style="text-align:right">Amount</th></tr></thead>
            <tbody>
              ${invPayments.map(p => `
                <tr>
                  <td>${dt(p.date)}</td>
                  <td><span class="badge badge-paid">${(p.mode || 'bank').toUpperCase()}</span></td>
                  <td style="font-family:monospace;font-size:.72rem">${p.reference || '-'}</td>
                  <td style="text-align:right;font-weight:600;color:var(--green)">${fmt(p.amount)}</td>
                </tr>
              `).join('')}
            </tbody>
          </table>` : `<div style="padding:20px;text-align:center;color:var(--text-xs);font-size:.82rem">No payments recorded yet</div>`}
        </div>
      </div>

      <!-- Payment Recording Form (hidden initially) -->
      <div class="card" id="payment-form-card" style="display:none;margin-top:20px;max-width:600px">
        <div class="card-header"><span class="card-title">Record Payment</span></div>
        <div class="form-section">
          <div class="form-row">
            <div class="form-group">
              <label class="form-label">Amount (₹)</label>
              <input class="form-control" type="number" step="0.01" id="pay-amount" value="${(balanceDue / 100).toFixed(2)}">
            </div>
            <div class="form-group">
              <label class="form-label">Mode</label>
              <select class="form-control" id="pay-mode">
                <option value="bank_transfer">Bank Transfer</option>
                <option value="upi">UPI</option>
                <option value="cash">Cash</option>
                <option value="cheque">Cheque</option>
              </select>
            </div>
          </div>
          <div class="form-group">
            <label class="form-label">UTR / Reference</label>
            <input class="form-control" id="pay-ref" placeholder="Transaction reference number">
          </div>
          <button class="btn btn-primary" onclick="InvoicesModule.recordPayment(${inv.id}, ${inv.customer_id || 0})">Submit Payment</button>
        </div>
      </div>
    `;
  },

  showRecordPayment(invoiceId, balance) {
    const card = document.getElementById('payment-form-card');
    if (card) card.style.display = card.style.display === 'none' ? 'block' : 'none';
  },

  async recordPayment(invoiceId, customerId) {
    const amount = Number(document.getElementById('pay-amount')?.value) ;
    if (amount <= 0) { QManage.toast('Enter a valid amount', 'error'); return; }

    try {
      await API.post('/payments/received', {
        invoice_id: invoiceId,
        customer_id: customerId,
        amount,
        mode: document.getElementById('pay-mode')?.value || 'bank_transfer',
        reference: document.getElementById('pay-ref')?.value || '',
        date: Math.floor(Date.now() / 1000)
      });
      QManage.toast('Payment recorded & ledger updated', 'success');
      QManage.route(); // Reload the page
    } catch(e) {
      QManage.toast('Failed: ' + e.message, 'error');
    }
  },

  async downloadPDF(id) {
    if (typeof window.jspdf === 'undefined') {
      QManage.toast('PDF library loading...', 'error');
      return;
    }
    
    let invoices = [];
    try { invoices = await API.get('/invoices'); } catch(e) {}
    const inv = invoices.find(i => i.id == id);
    if (!inv) return;
    
    const { jsPDF } = window.jspdf;
    const doc = new jsPDF();
    const fmt = (n) => 'Rs. ' + Number(n || 0).toLocaleString('en-IN', { minimumFractionDigits: 2 });
    const dt = (ts) => ts ? new Date(ts * 1000).toLocaleDateString('en-IN') : '-';
    
    // Header
    doc.setFontSize(22);
    doc.text('INVOICE', 14, 20);
    
    doc.setFontSize(10);
    doc.setTextColor(100);
    doc.text('Invoice #: ' + inv.invoice_num, 14, 30);
    doc.text('Date: ' + dt(inv.date), 14, 35);
    if (inv.due_date) doc.text('Due Date: ' + dt(inv.due_date), 14, 40);
    
    // Billed To
    doc.setTextColor(0);
    doc.setFontSize(12);
    doc.text('Billed To:', 14, 55);
    doc.setFontSize(10);
    doc.text(inv.customer_name || 'Customer', 14, 62);
    
    // Line items table
    const tableData = [
       ['Description', 'Qty', 'Rate', 'Amount'],
       ['Professional Services', '1', fmt(inv.subtotal || inv.total), fmt(inv.subtotal || inv.total)]
    ];
    
    doc.autoTable({
      startY: 75,
      head: [tableData[0]],
      body: [tableData[1]],
      theme: 'grid',
      headStyles: { fillColor: [52, 152, 219] }
    });
    
    const finalY = doc.lastAutoTable.finalY || 100;
    doc.setTextColor(100);
    doc.text('Subtotal: ' + fmt(inv.subtotal || inv.total), 140, finalY + 10);
    doc.text('Tax (18%): ' + fmt(inv.tax || 0), 140, finalY + 16);
    
    doc.setTextColor(0);
    doc.setFontSize(12);
    doc.setFont(undefined, 'bold');
    doc.text('Total: ' + fmt(inv.total), 140, finalY + 24);
    
    doc.save(inv.invoice_num + '.pdf');
  }
};
