/* Q Manage — Contacts Module (C API Integrated) */
const ContactsModule = {
  async renderList(typeFilter) {
    let contacts = [];
    try { contacts = await API.get('/contacts'); } catch(e) { /* offline */ }

    if (typeFilter) {
      contacts = contacts.filter(c => c.type === typeFilter);
    }

    const customers = contacts.filter(c => c.type === 'customer' || c.type === 'both');
    const vendors = contacts.filter(c => c.type === 'vendor' || c.type === 'both');

    return `
      <div class="page-header">
        <h1 class="page-title">Contacts</h1>
        <div class="page-actions">
          <button class="btn btn-primary" onclick="ContactsModule.showCreateForm()">
            <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/></svg>
            New Contact
          </button>
        </div>
      </div>

      <div class="stats-grid" style="margin-bottom:24px">
        <div class="stat-card blue">
          <span class="stat-label">Total Contacts</span>
          <span class="stat-value">${contacts.length}</span>
        </div>
        <div class="stat-card green">
          <span class="stat-label">Customers</span>
          <span class="stat-value">${customers.length}</span>
        </div>
        <div class="stat-card yellow">
          <span class="stat-label">Vendors</span>
          <span class="stat-value">${vendors.length}</span>
        </div>
      </div>

      <div class="table-container">
        <div class="table-toolbar">
          <div class="table-filters">
            <button class="filter-btn ${!typeFilter ? 'active':''}" onclick="location.hash='#/contacts'">All (${contacts.length})</button>
            <button class="filter-btn ${typeFilter==='customer'?'active':''}" onclick="location.hash='#/contacts?type=customer'">Customers</button>
            <button class="filter-btn ${typeFilter==='vendor'?'active':''}" onclick="location.hash='#/contacts?type=vendor'">Vendors</button>
          </div>
        </div>
        ${contacts.length ? `
        <table class="data-table">
          <thead><tr><th>Name</th><th>Type</th><th>Email</th><th>Phone</th><th>GSTIN</th></tr></thead>
          <tbody>
            ${contacts.map(c => `
              <tr style="cursor:pointer">
                <td style="font-weight:600;color:var(--primary)">${c.display_name}</td>
                <td><span class="badge badge-${c.type==='customer'?'paid':c.type==='vendor'?'sent':'partial'}">${(c.type||'customer').toUpperCase()}</span></td>
                <td>${c.email || '-'}</td>
                <td>${c.phone || '-'}</td>
                <td style="font-family:monospace;font-size:.72rem">${c.gstin || '-'}</td>
              </tr>
            `).join('')}
          </tbody>
        </table>` : `
        <div class="empty-state">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" style="width:64px;height:64px"><path d="M17 21v-2a4 4 0 0 0-4-4H5a4 4 0 0 0-4 4v2"/><circle cx="9" cy="7" r="4"/><path d="M23 21v-2a4 4 0 0 0-3-3.87"/><path d="M16 3.13a4 4 0 0 1 0 7.75"/></svg>
          <h3>No contacts yet</h3>
          <p>Add your customers and vendors to start creating invoices.</p>
          <button onclick="ContactsModule.showCreateForm()" class="btn btn-primary">+ Add Contact</button>
        </div>`}
      </div>`;
  },

  showCreateForm(type) {
    const content = document.getElementById('page-content');
    content.innerHTML = `
      <div class="page-header">
        <h1 class="page-title">New Contact</h1>
        <div class="page-actions">
          <a href="#/contacts" class="btn btn-secondary">Cancel</a>
          <button class="btn btn-primary" onclick="ContactsModule.save()">Save Contact</button>
        </div>
      </div>
      <div class="card" style="max-width:700px">
        <div class="form-section">
          <div class="form-section-title">Basic Information</div>
          <div class="form-row">
            <div class="form-group">
              <label class="form-label required">Contact Type</label>
              <select class="form-control" id="ct-type">
                <option value="customer" ${type==='customer'?'selected':''}>Customer</option>
                <option value="vendor" ${type==='vendor'?'selected':''}>Vendor</option>
                <option value="both">Both</option>
              </select>
            </div>
            <div class="form-group">
              <label class="form-label required">Display Name</label>
              <input class="form-control" id="ct-name" placeholder="Business or person name">
            </div>
          </div>
          <div class="form-row">
            <div class="form-group">
              <label class="form-label">Email</label>
              <input class="form-control" id="ct-email" type="email" placeholder="email@example.com">
            </div>
            <div class="form-group">
              <label class="form-label">Phone</label>
              <input class="form-control" id="ct-phone" placeholder="+91 9876543210">
            </div>
          </div>
        </div>
        <div class="form-section">
          <div class="form-section-title">Tax Information</div>
          <div class="form-row">
            <div class="form-group">
              <label class="form-label">GSTIN</label>
              <input class="form-control" id="ct-gstin" placeholder="22AAAAA0000A1Z5" maxlength="15" style="text-transform:uppercase">
            </div>
            <div class="form-group">
              <label class="form-label">Opening Balance (₹)</label>
              <input class="form-control" type="number" step="0.01" id="ct-balance" value="0" placeholder="0.00">
            </div>
          </div>
        </div>
      </div>`;
  },

  async save() {
    const name = document.getElementById('ct-name')?.value?.trim();
    if (!name) { QManage.toast('Display name is required', 'error'); return; }

    try {
      await API.post('/contacts', {
        display_name: name,
        type: document.getElementById('ct-type')?.value || 'customer',
        email: document.getElementById('ct-email')?.value?.trim() || '',
        phone: document.getElementById('ct-phone')?.value?.trim() || '',
        gstin: document.getElementById('ct-gstin')?.value?.trim().toUpperCase() || ''
      });
      QManage.toast(`Contact "${name}" created`, 'success');
      location.hash = '#/contacts';
    } catch(e) {
      QManage.toast('Save failed: ' + e.message, 'error');
    }
  },

  async renderForm(id, params) { this.showCreateForm(params?.type); return ''; },

  async renderView(id) {
    let contacts = [];
    let invoices = [];
    let bills = [];
    try { contacts = await API.get('/contacts'); } catch(e) {}
    try { invoices = await API.get('/invoices'); } catch(e) {}
    try { bills = await API.get('/bills'); } catch(e) {}

    const contact = contacts.find(c => c.id == id);
    if (!contact) {
      return `<div class="empty-state"><h3>Contact not found</h3><a href="#/contacts" class="btn btn-secondary">Back</a></div>`;
    }

    const fmt = (n) => '₹' + Number(n || 0).toLocaleString('en-IN', { minimumFractionDigits: 2 });
    const dt = (ts) => ts ? new Date(ts * 1000).toLocaleDateString('en-IN', { day: '2-digit', month: 'short', year: 'numeric' }) : '-';

    // Filter transactions for this contact
    const contactInvoices = invoices.filter(i => i.customer_id == id);
    const contactBills = bills.filter(b => b.vendor_id == id);
    const totalInvoiced = contactInvoices.reduce((s, i) => s + (i.total || 0), 0);
    const totalBilled = contactBills.reduce((s, b) => s + (b.total || 0), 0);
    const totalReceivable = contactInvoices.filter(i => i.status !== 'paid' && i.status !== 'void').reduce((s, i) => s + (i.balance_due || 0), 0);
    const totalPayable = contactBills.filter(b => b.status !== 'paid' && b.status !== 'void').reduce((s, b) => s + (b.balance_due || 0), 0);

    return `
      <div class="page-header">
        <h1 class="page-title">${contact.display_name}</h1>
        <div class="page-actions">
          <a href="#/contacts" class="btn btn-secondary">Back to Contacts</a>
          <button class="btn btn-primary" onclick="ContactsModule.editContact(${id})">Edit</button>
        </div>
      </div>

      <div class="stats-grid">
        <div class="stat-card blue"><span class="stat-label">TOTAL INVOICED</span><span class="stat-value">${fmt(totalInvoiced)}</span></div>
        <div class="stat-card red"><span class="stat-label">RECEIVABLE</span><span class="stat-value">${fmt(totalReceivable)}</span></div>
        <div class="stat-card green"><span class="stat-label">TOTAL BILLS</span><span class="stat-value">${fmt(totalBilled)}</span></div>
        <div class="stat-card yellow"><span class="stat-label">PAYABLE</span><span class="stat-value">${fmt(totalPayable)}</span></div>
      </div>

      <div class="charts-grid">
        <div class="card">
          <div class="card-header"><span class="card-title">Contact Information</span></div>
          <div style="padding:8px 0">
            <div style="display:grid;grid-template-columns:120px 1fr;gap:8px;font-size:.85rem">
              <span style="font-weight:600;color:var(--text-s)">Type</span>
              <span><span class="badge badge-${contact.type === 'vendor' ? 'sent' : 'paid'}">${(contact.type || 'customer').toUpperCase()}</span></span>
              <span style="font-weight:600;color:var(--text-s)">Company</span><span>${contact.company_name || '-'}</span>
              <span style="font-weight:600;color:var(--text-s)">Email</span><span>${contact.email ? `<a href="mailto:${contact.email}" style="color:var(--primary)">${contact.email}</a>` : '-'}</span>
              <span style="font-weight:600;color:var(--text-s)">Phone</span><span>${contact.phone || '-'}</span>
              <span style="font-weight:600;color:var(--text-s)">GSTIN</span><span style="font-family:monospace">${contact.gstin || '-'}</span>
              <span style="font-weight:600;color:var(--text-s)">PAN</span><span style="font-family:monospace">${contact.pan || '-'}</span>
              <span style="font-weight:600;color:var(--text-s)">Address</span><span>${[contact.billing_address, contact.billing_city, contact.billing_state].filter(Boolean).join(', ') || '-'}</span>
              <span style="font-weight:600;color:var(--text-s)">Created</span><span>${dt(contact.created_at)}</span>
            </div>
          </div>
        </div>

        <div class="card">
          <div class="card-header"><span class="card-title">Quick Actions</span></div>
          <div style="display:grid;grid-template-columns:1fr;gap:8px;padding:8px 0">
            <a href="#/invoices/new" class="btn btn-secondary" style="justify-content:center;padding:12px">+ Create Invoice</a>
            <a href="#/quotes" class="btn btn-secondary" style="justify-content:center;padding:12px">+ Create Quote</a>
            <a href="#/bills" class="btn btn-secondary" style="justify-content:center;padding:12px">+ Create Bill</a>
          </div>
        </div>
      </div>

      ${contactInvoices.length ? `
      <div class="card" style="margin-top:20px">
        <div class="card-header"><span class="card-title">Invoices (${contactInvoices.length})</span></div>
        <table class="data-table">
          <thead><tr><th>Date</th><th>Invoice #</th><th>Status</th><th style="text-align:right">Total</th><th style="text-align:right">Balance</th></tr></thead>
          <tbody>
            ${contactInvoices.map(inv => `
              <tr>
                <td>${dt(inv.date)}</td>
                <td style="font-weight:600;color:var(--primary)">${inv.invoice_num}</td>
                <td><span class="badge badge-${inv.status || 'draft'}">${(inv.status || 'draft').toUpperCase()}</span></td>
                <td style="text-align:right">${fmt(inv.total)}</td>
                <td style="text-align:right;font-weight:600;color:${(inv.balance_due || 0) > 0 ? 'var(--red)' : 'var(--green)'}">${fmt(inv.balance_due)}</td>
              </tr>
            `).join('')}
          </tbody>
        </table>
      </div>` : ''}

      ${contactBills.length ? `
      <div class="card" style="margin-top:20px">
        <div class="card-header"><span class="card-title">Bills (${contactBills.length})</span></div>
        <table class="data-table">
          <thead><tr><th>Date</th><th>Bill #</th><th>Status</th><th style="text-align:right">Total</th><th style="text-align:right">Balance</th></tr></thead>
          <tbody>
            ${contactBills.map(b => `
              <tr>
                <td>${dt(b.date)}</td>
                <td style="font-weight:600">${b.bill_num}</td>
                <td><span class="badge badge-${b.status || 'draft'}">${(b.status || 'draft').toUpperCase()}</span></td>
                <td style="text-align:right">${fmt(b.total)}</td>
                <td style="text-align:right;font-weight:600;color:${(b.balance_due || 0) > 0 ? 'var(--red)' : 'var(--green)'}">${fmt(b.balance_due)}</td>
              </tr>
            `).join('')}
          </tbody>
        </table>
      </div>` : ''}
    `;
  },

  async editContact(id) {
    QManage.toast('Edit form loading...', 'info');
    // Re-use create form pre-filled — full edit needs GET /contacts/:id endpoint
    location.hash = `#/contacts/new`;
  }
};
