/* Q Manage — Dashboard Module (Production Grade) */
const DashboardModule = {
  _stats: null,
  _chart: null,

  afterRender() {
    if (!this._stats || typeof Chart === 'undefined') return;
    const s = this._stats;
    const income   = s.total_income   || 0;
    const expenses = s.total_expenses || 0;
    const profit   = (s.net_profit != null) ? s.net_profit : (income - expenses);
    const ctx = document.getElementById('qm-cashflow-chart');
    if (!ctx) return;
    if (this._chart) { this._chart.destroy(); this._chart = null; }
    this._chart = new Chart(ctx, {
      type: 'bar',
      data: {
        labels: ['Income', 'Expenses', 'Net Profit'],
        datasets: [{
          data: [income / 100, expenses / 100, profit / 100],
          backgroundColor: ['#34a85344', '#ea433544', '#1a73e844'],
          borderColor:     ['#34a853',   '#ea4335',   '#1a73e8'],
          borderWidth: 2,
          borderRadius: 6,
        }]
      },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        plugins: { legend: { display: false } },
        scales: {
          y: {
            ticks: {
              callback: (v) => '₹' + Number(v).toLocaleString('en-IN'),
              font: { size: 11 }
            },
            grid: { color: 'rgba(0,0,0,0.05)' }
          },
          x: { grid: { display: false } }
        }
      }
    });
  },

  async render() {
    const isOnline = await API.checkHealth();

    if (!isOnline) {
      return `<div class="empty-state">
                <svg viewBox="0 0 24 24" width="64" height="64" fill="none" stroke="currentColor" stroke-width="1" style="margin-bottom:16px;opacity:.3"><circle cx="12" cy="12" r="10"/><line x1="12" y1="8" x2="12" y2="12"/><line x1="12" y1="16" x2="12.01" y2="16"/></svg>
                <h3>Backend Offline</h3>
                <p>The Q Manage C Engine is not responding on port 9741.<br>Please ensure the backend server is running.</p>
                <button onclick="location.reload()" class="btn btn-primary" style="margin-top:8px">Retry Connection</button>
              </div>`;
    }

    try {
      const stats = await API.get('/dashboard');

      this._stats = stats;
      const totalAssets = stats.total_assets || 0;
      const totalIncome = stats.total_income || 0;
      const totalExpenses = stats.total_expenses || 0;
      const totalLiabilities = stats.total_liabilities || 0;
      const netProfit = stats.net_profit || (totalIncome - totalExpenses);
      const counts = stats.counts || {};
      const recentInvoices = stats.recent_invoices || [];
      const pipeline = stats.pipeline || [];

      const fmt = (n) => '₹' + Number(n || 0).toLocaleString('en-IN', { minimumFractionDigits: 2 });
      const dt = (ts) => ts ? new Date(ts * 1000).toLocaleDateString('en-IN', { day: '2-digit', month: 'short' }) : '-';

      return `
        <div class="page-header">
          <h1 class="page-title">Dashboard</h1>
          <div class="page-actions">
            <span style="font-size:.78rem;color:var(--text-xs)">${new Date().toLocaleDateString('en-IN', { weekday: 'long', day: 'numeric', month: 'long', year: 'numeric' })}</span>
          </div>
        </div>

        <div class="stats-grid">
          <div class="stat-card blue" onclick="location.hash='#/invoices'" style="cursor:pointer">
            <span class="stat-label">RECEIVABLES</span>
            <span class="stat-value">${fmt(counts.total_receivable)}</span>
            <span class="stat-change">${counts.unpaid_invoices || 0} unpaid invoices</span>
          </div>
          <div class="stat-card red" onclick="location.hash='#/bills'" style="cursor:pointer">
            <span class="stat-label">PAYABLES</span>
            <span class="stat-value">${fmt(counts.total_payable)}</span>
            <span class="stat-change">${counts.unpaid_bills || 0} unpaid bills</span>
          </div>
          <div class="stat-card green" onclick="location.hash='#/reports/profit-and-loss'" style="cursor:pointer">
            <span class="stat-label">NET PROFIT</span>
            <span class="stat-value" style="color:${netProfit >= 0 ? 'var(--green)' : 'var(--red)'}">${fmt(netProfit)}</span>
            <span class="stat-change up">Income − Expenses</span>
          </div>
          <div class="stat-card yellow" onclick="location.hash='#/crm'" style="cursor:pointer">
            <span class="stat-label">CRM PIPELINE</span>
            <span class="stat-value">${counts.active_deals || 0}</span>
            <span class="stat-change">${counts.open_quotes || 0} open quotes</span>
          </div>
        </div>

        <div class="charts-grid">
          <div class="card">
            <div class="card-header">
              <span class="card-title">Cash Flow Overview</span>
            </div>
            <div style="position:relative;height:200px;padding:8px 0">
              <canvas id="qm-cashflow-chart"></canvas>
            </div>
          </div>

          <div class="card">
            <div class="card-header">
              <span class="card-title">Quick Actions</span>
            </div>
            <div style="display:grid;grid-template-columns:1fr 1fr;gap:10px;padding:8px 0">
              <a href="#/invoices/new" class="btn btn-secondary" style="justify-content:center;padding:14px">+ New Invoice</a>
              <a href="#/expenses/new" class="btn btn-secondary" style="justify-content:center;padding:14px">+ Record Expense</a>
              <a href="#/quotes" class="btn btn-secondary" style="justify-content:center;padding:14px">+ New Quote</a>
              <a href="#/crm" class="btn btn-secondary" style="justify-content:center;padding:14px">+ New Deal</a>
            </div>
          </div>
        </div>

        <!-- Recent Invoices + Alerts -->
        <div class="charts-grid" style="margin-top:0">
          <div class="card">
            <div class="card-header">
              <span class="card-title">Recent Invoices</span>
              <a href="#/invoices" class="btn btn-ghost btn-sm">View All</a>
            </div>
            ${recentInvoices.length ? `
            <table class="data-table">
              <thead><tr><th>Invoice</th><th>Customer</th><th>Status</th><th style="text-align:right">Amount</th></tr></thead>
              <tbody>
                ${recentInvoices.map(inv => `
                  <tr>
                    <td style="font-weight:600;color:var(--primary)">${inv.invoice_num}</td>
                    <td>${inv.customer || '-'}</td>
                    <td><span class="badge badge-${inv.status || 'draft'}">${(inv.status || 'draft').toUpperCase()}</span></td>
                    <td style="text-align:right;font-weight:500">${fmt(inv.total)}</td>
                  </tr>
                `).join('')}
              </tbody>
            </table>` : '<div class="empty-state" style="padding:30px"><p>No invoices yet. Create your first invoice!</p></div>'}
          </div>

          <div class="card">
            <div class="card-header">
              <span class="card-title">System Status</span>
            </div>
            <div style="padding:8px 0">
              <div style="display:flex;justify-content:space-between;align-items:center;padding:8px 0;border-bottom:1px solid var(--border-l)">
                <span style="font-size:.82rem">C Backend Engine</span>
                <span class="badge badge-paid">ONLINE</span>
              </div>
              <div style="display:flex;justify-content:space-between;align-items:center;padding:8px 0;border-bottom:1px solid var(--border-l)">
                <span style="font-size:.82rem">Contacts</span>
                <span style="font-weight:600;font-size:.85rem">${counts.total_contacts || 0}</span>
              </div>
              <div style="display:flex;justify-content:space-between;align-items:center;padding:8px 0;border-bottom:1px solid var(--border-l)">
                <span style="font-size:.82rem">Invoices</span>
                <span style="font-weight:600;font-size:.85rem">${counts.total_invoices || 0}</span>
              </div>
              <div style="display:flex;justify-content:space-between;align-items:center;padding:8px 0;border-bottom:1px solid var(--border-l)">
                <span style="font-size:.82rem">Unmatched Bank Tx</span>
                <span style="font-weight:600;font-size:.85rem;color:${(counts.unmatched_transactions || 0) > 0 ? 'var(--red)' : 'var(--green)'}">${counts.unmatched_transactions || 0}</span>
              </div>
              <div style="display:flex;justify-content:space-between;align-items:center;padding:8px 0">
                <span style="font-size:.82rem">Low Stock Items</span>
                <span style="font-weight:600;font-size:.85rem;color:${(counts.low_stock_items || 0) > 0 ? 'var(--red)' : 'var(--green)'}">${counts.low_stock_items || 0}</span>
              </div>
            </div>
          </div>
        </div>
      `;
    } catch (e) {
      console.error('Dashboard Load Error:', e);
      return `<div class="empty-state"><h3>Dashboard Error</h3><p>${e.message}</p><button onclick="location.reload()" class="btn btn-primary" style="margin-top:8px">Retry</button></div>`;
    }
  }
};
