/* Q Manage — CRM Deals Pipeline Module */
const CRMModule = {
  stages: ['contacted', 'qualified', 'quoted', 'negotiating', 'won', 'lost'],
  stageColors: { contacted: '#4285f4', qualified: '#fbbc04', quoted: '#34a853', negotiating: '#ea4335', won: '#0d652d', lost: '#9e9e9e' },

  async renderList() {
    let data = { pipeline: [], deals: [] };
    try { data = await API.get('/deals'); } catch(e) {}
    const deals = data.deals || [];
    const pipeline = data.pipeline || [];

    const fmt = (n) => '₹' + Number(n || 0).toLocaleString('en-IN', { minimumFractionDigits: 0 });
    const dt = (ts) => ts ? new Date(ts * 1000).toLocaleDateString('en-IN', { day: '2-digit', month: 'short', year: 'numeric' }) : '-';

    const pipelineTotals = {};
    pipeline.forEach(p => { pipelineTotals[p.stage] = { count: p.count, total: p.total }; });

    const activeDeals = deals.filter(d => d.stage !== 'won' && d.stage !== 'lost');
    const wonDeals = deals.filter(d => d.stage === 'won');
    const totalPipelineValue = activeDeals.reduce((s, d) => s + (d.amount || 0), 0);

    return `
      <div class="page-header">
        <h1 class="page-title">CRM Pipeline</h1>
        <div class="page-actions">
          <button class="btn btn-primary" onclick="CRMModule.showCreateForm()">
            <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/></svg>
            New Deal
          </button>
        </div>
      </div>

      <div class="stats-grid">
        <div class="stat-card blue">
          <span class="stat-label">PIPELINE VALUE</span>
          <span class="stat-value">${fmt(totalPipelineValue)}</span>
          <span class="stat-change">${activeDeals.length} active deals</span>
        </div>
        <div class="stat-card green">
          <span class="stat-label">WON</span>
          <span class="stat-value">${wonDeals.length}</span>
          <span class="stat-change up">${fmt(wonDeals.reduce((s, d) => s + (d.amount || 0), 0))} closed</span>
        </div>
        <div class="stat-card yellow">
          <span class="stat-label">QUALIFIED</span>
          <span class="stat-value">${deals.filter(d => d.stage === 'qualified').length}</span>
        </div>
        <div class="stat-card red">
          <span class="stat-label">NEGOTIATING</span>
          <span class="stat-value">${deals.filter(d => d.stage === 'negotiating').length}</span>
        </div>
      </div>

      <!-- Kanban Pipeline View -->
      <div style="display:flex;gap:12px;overflow-x:auto;padding-bottom:16px;min-height:300px">
        ${this.stages.filter(s => s !== 'won' && s !== 'lost').map(stage => `
          <div style="min-width:240px;flex:1;background:var(--bg);border-radius:var(--radius);padding:12px;border:1px solid var(--border-l)">
            <div style="display:flex;align-items:center;justify-content:space-between;margin-bottom:12px">
              <span style="font-size:.78rem;font-weight:700;text-transform:uppercase;color:${this.stageColors[stage]}">${stage}</span>
              <span style="font-size:.7rem;background:var(--bg-card);padding:2px 8px;border-radius:10px;border:1px solid var(--border)">${deals.filter(d => d.stage === stage).length}</span>
            </div>
            ${deals.filter(d => d.stage === stage).map(deal => `
              <div class="card" style="padding:12px;margin-bottom:8px;cursor:pointer" onclick="CRMModule.updateStage(${deal.id}, '${stage}')">
                <div style="font-weight:600;font-size:.82rem;margin-bottom:4px">${deal.title}</div>
                <div style="font-size:.72rem;color:var(--text-s)">${deal.contact_name || 'No contact'}</div>
                <div style="display:flex;justify-content:space-between;margin-top:8px;align-items:center">
                  <span style="font-weight:700;color:var(--primary);font-size:.8rem">${fmt(deal.amount)}</span>
                  <span style="font-size:.65rem;color:var(--text-xs)">${deal.probability || 50}%</span>
                </div>
              </div>
            `).join('') || '<div style="text-align:center;padding:20px;color:var(--text-xs);font-size:.75rem">No deals</div>'}
          </div>
        `).join('')}
      </div>

      <!-- Deals Table -->
      <div class="table-container" style="margin-top:24px">
        <div class="table-toolbar">
          <div class="table-filters">
            <button class="filter-btn active">All (${deals.length})</button>
            <button class="filter-btn">Active</button>
            <button class="filter-btn">Won</button>
            <button class="filter-btn">Lost</button>
          </div>
        </div>
        ${deals.length ? `
        <table class="data-table">
          <thead><tr><th>Deal</th><th>Contact</th><th>Stage</th><th>Source</th><th>Probability</th><th style="text-align:right">Amount</th></tr></thead>
          <tbody>
            ${deals.map(d => `
              <tr>
                <td style="font-weight:600;color:var(--primary)">${d.title}</td>
                <td>${d.contact_name || '-'}</td>
                <td><span class="badge badge-${d.stage === 'won' ? 'paid' : d.stage === 'lost' ? 'void' : 'sent'}">${d.stage.toUpperCase()}</span></td>
                <td>${d.source || '-'}</td>
                <td>${d.probability || 50}%</td>
                <td style="text-align:right;font-weight:600">${fmt(d.amount)}</td>
              </tr>
            `).join('')}
          </tbody>
        </table>` : `
        <div class="empty-state">
          <h3>No deals yet</h3>
          <p>Start tracking your sales pipeline. Add leads and convert them to deals.</p>
          <button onclick="CRMModule.showCreateForm()" class="btn btn-primary">+ New Deal</button>
        </div>`}
      </div>`;
  },

  async showCreateForm() {
    let contacts = [];
    try { contacts = await API.get('/contacts'); } catch(e) {}

    const content = document.getElementById('page-content');
    content.innerHTML = `
      <div class="page-header">
        <h1 class="page-title">New Deal</h1>
        <div class="page-actions">
          <a href="#/crm" class="btn btn-secondary">Cancel</a>
          <button class="btn btn-primary" onclick="CRMModule.save()">Create Deal</button>
        </div>
      </div>
      <div class="card" style="max-width:700px">
        <div class="form-section">
          <div class="form-section-title">Deal Information</div>
          <div class="form-group">
            <label class="form-label required">Deal Title</label>
            <input class="form-control" id="deal-title" placeholder="e.g. Cotton Export — Ramesh Textiles">
          </div>
          <div class="form-row">
            <div class="form-group">
              <label class="form-label">Contact</label>
              <select class="form-control" id="deal-contact">
                <option value="">Select Contact</option>
                ${contacts.map(c => `<option value="${c.id}">${c.display_name}</option>`).join('')}
              </select>
            </div>
            <div class="form-group">
              <label class="form-label required">Amount (₹)</label>
              <input class="form-control" type="number" id="deal-amount" placeholder="0">
            </div>
          </div>
          <div class="form-row">
            <div class="form-group">
              <label class="form-label">Stage</label>
              <select class="form-control" id="deal-stage">
                <option value="contacted">Contacted</option>
                <option value="qualified">Qualified</option>
                <option value="quoted">Quoted</option>
                <option value="negotiating">Negotiating</option>
              </select>
            </div>
            <div class="form-group">
              <label class="form-label">Source</label>
              <select class="form-control" id="deal-source">
                <option value="referral">Referral</option>
                <option value="walk_in">Walk-in</option>
                <option value="cold_call">Cold Call</option>
                <option value="website">Website</option>
              </select>
            </div>
          </div>
          <div class="form-row">
            <div class="form-group">
              <label class="form-label">Probability (%)</label>
              <input class="form-control" type="number" id="deal-prob" value="50" min="0" max="100">
            </div>
            <div class="form-group">
              <label class="form-label">Expected Close</label>
              <input class="form-control" type="date" id="deal-close">
            </div>
          </div>
          <div class="form-group">
            <label class="form-label">Notes</label>
            <textarea class="form-control" id="deal-notes" placeholder="Deal details, negotiation notes..."></textarea>
          </div>
        </div>
      </div>`;
  },

  async save() {
    const title = document.getElementById('deal-title')?.value?.trim();
    if (!title) { QManage.toast('Deal title is required', 'error'); return; }

    const closeDate = document.getElementById('deal-close')?.value;
    try {
      await API.post('/deals', {
        title,
        contact_id: parseInt(document.getElementById('deal-contact')?.value) || 0,
        amount: Number(document.getElementById('deal-amount')?.value) || 0,
        stage: document.getElementById('deal-stage')?.value || 'contacted',
        source: document.getElementById('deal-source')?.value || '',
        probability: Number(document.getElementById('deal-prob')?.value) || 50,
        expected_close: closeDate ? Math.floor(new Date(closeDate).getTime() / 1000) : 0,
        notes: document.getElementById('deal-notes')?.value || ''
      });
      QManage.toast(`Deal "${title}" created`, 'success');
      location.hash = '#/crm';
    } catch(e) {
      QManage.toast('Failed: ' + e.message, 'error');
    }
  },

  async updateStage(id, currentStage) {
    const next = { contacted: 'qualified', qualified: 'quoted', quoted: 'negotiating', negotiating: 'won' };
    const newStage = next[currentStage] || 'won';
    if (!confirm(`Move deal to "${newStage.toUpperCase()}"?`)) return;

    try {
      await API.put('/deals', { id, stage: newStage });
      QManage.toast(`Deal moved to ${newStage}`, 'success');
      QManage.route();
    } catch(e) {
      QManage.toast('Failed: ' + e.message, 'error');
    }
  }
};
