/* Q Manage — Leads Module (CRM Pre-deal Prospects) */
window.LeadsModule = {
  _statusColors: {
    new:        { bg: 'var(--blue-l)',   fg: 'var(--primary)' },
    contacted:  { bg: 'var(--orange-l)', fg: 'var(--orange)' },
    qualified:  { bg: 'var(--green-l)',  fg: 'var(--green)' },
    proposal:   { bg: '#e8eaf6',         fg: '#3949ab' },
    won:        { bg: 'var(--green-l)',  fg: 'var(--green)' },
    lost:       { bg: 'var(--red-l)',    fg: 'var(--red)' },
    converted:  { bg: '#e8f5e9',         fg: '#2e7d32' },
  },

  async renderList() {
    let leads = [];
    try {
      const resp = await API.get('/leads');
      leads = resp.leads || [];
    } catch (e) {
      return `<div class="alert alert-danger">Failed to load leads: ${e.message}</div>`;
    }

    const dt = (ts) => ts ? new Date(ts * 1000).toLocaleDateString('en-IN', { day: '2-digit', month: 'short', year: 'numeric' }) : '-';
    const sourceLabel = { website:'Website', referral:'Referral', cold_call:'Cold Call', walk_in:'Walk-In', social:'Social Media' };

    return `
      <div class="page-header">
        <h1 class="page-title">Leads</h1>
        <button class="btn btn-primary" onclick="LeadsModule.renderForm()">+ New Lead</button>
      </div>

      <div class="card">
        ${leads.length === 0 ? `
          <div class="empty-state" style="padding:40px">
            <p>No leads yet. Add your first prospect.</p>
          </div>
        ` : `
        <table class="data-table">
          <thead>
            <tr>
              <th>Name</th>
              <th>Company</th>
              <th>Source</th>
              <th>Contact</th>
              <th>Status</th>
              <th>Created</th>
              <th>Actions</th>
            </tr>
          </thead>
          <tbody>
            ${leads.map(l => {
              const sc = this._statusColors[l.status] || { bg: 'var(--gray-2)', fg: 'var(--text)' };
              return `
                <tr>
                  <td style="font-weight:600">${l.name}</td>
                  <td>${l.company || '-'}</td>
                  <td>${sourceLabel[l.source] || l.source || '-'}</td>
                  <td>
                    ${l.email ? `<div style="font-size:.78rem">${l.email}</div>` : ''}
                    ${l.phone ? `<div style="font-size:.78rem;color:var(--text-s)">${l.phone}</div>` : ''}
                  </td>
                  <td>
                    <select onchange="LeadsModule.updateStatus(${l.id}, this.value)"
                      style="background:${sc.bg};color:${sc.fg};border:none;padding:3px 6px;border-radius:4px;font-size:.75rem;font-weight:600;cursor:pointer">
                      ${['new','contacted','qualified','proposal','won','lost'].map(s =>
                        `<option value="${s}" ${l.status===s?'selected':''}>${s.toUpperCase()}</option>`
                      ).join('')}
                    </select>
                  </td>
                  <td style="font-size:.78rem;color:var(--text-s)">${dt(l.created_at)}</td>
                  <td style="display:flex;gap:6px">
                    ${l.status !== 'converted' ? `
                      <button class="btn btn-secondary" style="padding:3px 8px;font-size:.75rem;color:var(--green);border-color:var(--green)"
                        onclick="LeadsModule.convertToDeal(${l.id}, '${l.name.replace(/'/g,"\\'")}')">
                        → Deal
                      </button>` : `<span style="font-size:.75rem;color:var(--green)">Converted</span>`}
                    <button class="btn btn-secondary" style="padding:3px 8px;font-size:.75rem;color:var(--red);border-color:var(--red)"
                      onclick="LeadsModule.deleteLead(${l.id})">Delete</button>
                  </td>
                </tr>
              `;
            }).join('')}
          </tbody>
        </table>`}
      </div>
    `;
  },

  renderForm() {
    document.getElementById('page-content').innerHTML = `
      <div class="page-header">
        <h1 class="page-title">New Lead</h1>
        <button class="btn btn-secondary" onclick="window.location.hash='#/leads'">Back</button>
      </div>
      <div class="card" style="max-width:600px">
        <div class="form-group">
          <label class="form-label required">Full Name</label>
          <input type="text" id="ld-name" class="form-control" placeholder="Prospect name">
        </div>
        <div class="form-group">
          <label class="form-label">Company</label>
          <input type="text" id="ld-company" class="form-control" placeholder="Company name">
        </div>
        <div style="display:grid;grid-template-columns:1fr 1fr;gap:16px">
          <div class="form-group">
            <label class="form-label">Email</label>
            <input type="email" id="ld-email" class="form-control" placeholder="email@example.com">
          </div>
          <div class="form-group">
            <label class="form-label">Phone</label>
            <input type="tel" id="ld-phone" class="form-control" placeholder="+91 9876543210">
          </div>
        </div>
        <div class="form-group">
          <label class="form-label">Source</label>
          <select id="ld-source" class="form-control">
            <option value="website">Website</option>
            <option value="referral">Referral</option>
            <option value="cold_call">Cold Call</option>
            <option value="walk_in">Walk-In</option>
            <option value="social">Social Media</option>
          </select>
        </div>
        <div class="form-group">
          <label class="form-label">Notes</label>
          <textarea id="ld-notes" class="form-control" rows="3" placeholder="Any context about this lead..."></textarea>
        </div>
        <div style="margin-top:24px;text-align:right">
          <button class="btn btn-primary" onclick="LeadsModule.save()">Save Lead</button>
        </div>
      </div>
    `;
  },

  async save() {
    const name    = document.getElementById('ld-name').value.trim();
    const company = document.getElementById('ld-company').value.trim();
    const email   = document.getElementById('ld-email').value.trim();
    const phone   = document.getElementById('ld-phone').value.trim();
    const source  = document.getElementById('ld-source').value;
    const notes   = document.getElementById('ld-notes').value.trim();

    if (!name) { QManage.toast('Name is required', 'error'); return; }

    try {
      await API.post('/leads', { name, company, email, phone, source, notes });
      QManage.toast('Lead created', 'success');
      window.location.hash = '#/leads';
    } catch (e) {
      QManage.toast('Failed to save: ' + e.message, 'error');
    }
  },

  async updateStatus(id, status) {
    try {
      await API.put('/leads/' + id, { status });
      QManage.toast('Status updated to ' + status.toUpperCase(), 'success');
    } catch (e) {
      QManage.toast('Error: ' + e.message, 'error');
    }
  },

  async convertToDeal(id, name) {
    if (!confirm(`Convert "${name}" to a Deal in the CRM pipeline?`)) return;
    try {
      const resp = await API.put('/leads/' + id, { convert_to_deal: true });
      QManage.toast('Converted! Deal #' + resp.deal_id + ' created.', 'success');
      const html = await this.renderList();
      document.getElementById('page-content').innerHTML = html;
    } catch (e) {
      QManage.toast('Error: ' + e.message, 'error');
    }
  },

  async deleteLead(id) {
    if (!confirm('Remove this lead?')) return;
    try {
      await API.del('/leads/' + id);
      QManage.toast('Lead removed', 'success');
      const html = await this.renderList();
      document.getElementById('page-content').innerHTML = html;
    } catch (e) {
      QManage.toast('Error: ' + e.message, 'error');
    }
  }
};
