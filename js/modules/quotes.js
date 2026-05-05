/* Q Manage — Quotes / Estimates Module */
const QuotesModule = {
  async renderList() {
    let quotes = [];
    try { quotes = await API.get('/quotes'); } catch(e) {}

    const fmt = (n) => '₹' + Number(n || 0).toLocaleString('en-IN', { minimumFractionDigits: 2 });
    const dt = (ts) => ts ? new Date(ts * 1000).toLocaleDateString('en-IN', { day: '2-digit', month: 'short', year: 'numeric' }) : '-';

    return `
      <div class="page-header">
        <h1 class="page-title">Quotes / Estimates</h1>
        <div class="page-actions">
          <button class="btn btn-primary" onclick="QuotesModule.showCreateForm()">
            <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/></svg>
            New Quote
          </button>
        </div>
      </div>
      <div class="stats-grid">
        <div class="stat-card blue"><span class="stat-label">Total Quotes</span><span class="stat-value">${quotes.length}</span></div>
        <div class="stat-card green"><span class="stat-label">Accepted</span><span class="stat-value">${quotes.filter(q => q.status === 'accepted').length}</span></div>
        <div class="stat-card yellow"><span class="stat-label">Pending</span><span class="stat-value">${quotes.filter(q => q.status === 'draft' || q.status === 'sent').length}</span></div>
      </div>
      <div class="table-container">
        ${quotes.length ? `
        <table class="data-table">
          <thead><tr><th>Date</th><th>Quote #</th><th>Customer</th><th>Status</th><th style="text-align:right">Amount</th><th>Action</th></tr></thead>
          <tbody>
            ${quotes.map(q => `
              <tr>
                <td>${dt(q.date)}</td>
                <td style="font-weight:600;color:var(--primary)">${q.quote_num}</td>
                <td>${q.customer_name || '-'}</td>
                <td><span class="badge badge-${q.status === 'accepted' ? 'paid' : q.status === 'converted' ? 'active' : q.status === 'declined' ? 'void' : 'sent'}">${(q.status||'draft').toUpperCase()}</span></td>
                <td style="text-align:right;font-weight:600">${fmt(q.total)}</td>
                <td>${q.status !== 'converted' && q.status !== 'declined' ?
                  `<button class="btn btn-sm btn-primary" onclick="QuotesModule.convert(${q.id})">→ Invoice</button>` :
                  (q.converted_invoice_id ? '<span style="color:var(--green);font-size:.72rem">Converted</span>' : '')}
                  <button class="btn btn-sm btn-secondary" style="margin-left:4px" onclick="QuotesModule.downloadPDF(${q.id})">PDF</button>
                </td>
              </tr>
            `).join('')}
          </tbody>
        </table>` : `
        <div class="empty-state">
          <h3>No quotes yet</h3>
          <p>Create estimates for clients. One-click conversion to invoices.</p>
          <button onclick="QuotesModule.showCreateForm()" class="btn btn-primary">+ New Quote</button>
        </div>`}
      </div>`;
  },

  async showCreateForm() {
    let contacts = [];
    try { contacts = (await API.get('/contacts')).filter(c => c.type === 'customer' || c.type === 'both'); } catch(e) {}

    document.getElementById('page-content').innerHTML = `
      <div class="page-header">
        <h1 class="page-title">New Quote</h1>
        <div class="page-actions">
          <a href="#/quotes" class="btn btn-secondary">Cancel</a>
          <button class="btn btn-primary" onclick="QuotesModule.save()">Save Quote</button>
        </div>
      </div>
      <div class="card" style="max-width:700px">
        <div class="form-section">
          <div class="form-row">
            <div class="form-group">
              <label class="form-label required">Quote Number</label>
              <input class="form-control" id="qt-num" value="QT-${String(Date.now()).slice(-5)}">
            </div>
            <div class="form-group">
              <label class="form-label required">Date</label>
              <input class="form-control" type="date" id="qt-date" value="${new Date().toISOString().split('T')[0]}">
            </div>
          </div>
          <div class="form-row">
            <div class="form-group">
              <label class="form-label required">Customer</label>
              <select class="form-control" id="qt-customer">
                <option value="">Select Customer</option>
                ${contacts.map(c => `<option value="${c.id}">${c.display_name}</option>`).join('')}
              </select>
            </div>
            <div class="form-group">
              <label class="form-label required">Amount (₹)</label>
              <input class="form-control" type="number" step="0.01" id="qt-amount" placeholder="0.00">
            </div>
          </div>
        </div>
      </div>`;
  },

  async renderForm() {
    await this.showCreateForm();
    return '';
  },

  async save() {
    const num = document.getElementById('qt-num')?.value?.trim();
    const customer = document.getElementById('qt-customer')?.value;
    const amount = Number(document.getElementById('qt-amount')?.value) ;

    if (!num || !customer || amount <= 0) { QManage.toast('Fill required fields', 'error'); return; }

    try {
      await API.post('/quotes', {
        quote_num: num,
        customer_id: parseInt(customer),
        total: amount,
        date: Math.floor(new Date(document.getElementById('qt-date')?.value).getTime() / 1000)
      });
      QManage.toast(`Quote ${num} created`, 'success');
      location.hash = '#/quotes';
    } catch(e) { QManage.toast('Failed: ' + e.message, 'error'); }
  },

  async convert(quoteId) {
    if (!confirm('Convert this quote to an invoice? This will post to the ledger.')) return;
    try {
      const res = await API.post('/quotes/convert', { quote_id: quoteId });
      QManage.toast(`Converted! Invoice ${res.invoice_num} created.`, 'success');
      QManage.route();
    } catch(e) { QManage.toast('Failed: ' + e.message, 'error'); }
  },

  async downloadPDF(id) {
    if (typeof window.jspdf === 'undefined') {
      QManage.toast('PDF library loading...', 'error');
      return;
    }
    let quotes = [];
    try { quotes = await API.get('/quotes'); } catch(e) {}
    const q = quotes.find(i => i.id == id);
    if (!q) return;
    
    const { jsPDF } = window.jspdf;
    const doc = new jsPDF();
    const fmt = (n) => 'Rs. ' + Number(n || 0).toLocaleString('en-IN', { minimumFractionDigits: 2 });
    const dt = (ts) => ts ? new Date(ts * 1000).toLocaleDateString('en-IN') : '-';
    
    doc.setFontSize(22);
    doc.text('QUOTE / ESTIMATE', 14, 20);
    
    doc.setFontSize(10);
    doc.setTextColor(100);
    doc.text('Quote #: ' + q.quote_num, 14, 30);
    doc.text('Date: ' + dt(q.date), 14, 35);
    
    doc.setTextColor(0);
    doc.setFontSize(12);
    doc.text('Prepared For:', 14, 55);
    doc.setFontSize(10);
    doc.text(q.customer_name || 'Customer', 14, 62);
    
    const tableData = [
       ['Description', 'Amount'],
       ['Professional Services', fmt(q.total)]
    ];
    
    doc.autoTable({
      startY: 75,
      head: [tableData[0]],
      body: [tableData[1]],
      theme: 'grid',
      headStyles: { fillColor: [46, 204, 113] }
    });
    
    const finalY = doc.lastAutoTable.finalY || 100;
    doc.setTextColor(0);
    doc.setFontSize(12);
    doc.setFont(undefined, 'bold');
    doc.text('Total Estimate: ' + fmt(q.total), 140, finalY + 14);
    
    doc.save(q.quote_num + '.pdf');
  }
};
