/* Q Manage — Reports Module (Full Ledger-Powered Analytics) */
const ReportsModule = {
  _salesData: null,
  _expenseData: null,
  _salesChart: null,
  _expenseChart: null,

  afterRender(action) {
    if (typeof Chart === 'undefined') return;
    if (action === 'sales-by-customer' && this._salesData) {
      const ctx = document.getElementById('qm-sales-chart');
      if (!ctx) return;
      if (this._salesChart) { this._salesChart.destroy(); this._salesChart = null; }
      const top = this._salesData.slice(0, 8);
      this._salesChart = new Chart(ctx, {
        type: 'bar',
        data: {
          labels: top.map(d => d.customer),
          datasets: [{
            label: 'Total Sales (₹)',
            data: top.map(d => (d.total_sales || 0) / 100),
            backgroundColor: '#1a73e844',
            borderColor: '#1a73e8',
            borderWidth: 2,
            borderRadius: 4,
          }]
        },
        options: {
          indexAxis: 'y',
          responsive: true,
          maintainAspectRatio: false,
          plugins: { legend: { display: false } },
          scales: {
            x: { ticks: { callback: (v) => '₹' + Number(v).toLocaleString('en-IN'), font: { size: 10 } } },
            y: { ticks: { font: { size: 11 } }, grid: { display: false } }
          }
        }
      });
    }
    if (action === 'expense-by-category' && this._expenseData) {
      const ctx = document.getElementById('qm-expense-chart');
      if (!ctx) return;
      if (this._expenseChart) { this._expenseChart.destroy(); this._expenseChart = null; }
      const colors = ['#ea4335','#fbbc04','#34a853','#4285f4','#9c27b0','#ff6d00','#00bcd4','#795548'];
      this._expenseChart = new Chart(ctx, {
        type: 'doughnut',
        data: {
          labels: this._expenseData.map(d => d.category),
          datasets: [{
            data: this._expenseData.map(d => (d.total_amount || 0) / 100),
            backgroundColor: colors.slice(0, this._expenseData.length),
            borderWidth: 2,
          }]
        },
        options: {
          responsive: true,
          maintainAspectRatio: false,
          plugins: {
            legend: { position: 'right', labels: { font: { size: 11 } } },
            tooltip: { callbacks: { label: (ctx) => ' ₹' + Number(ctx.parsed).toLocaleString('en-IN') } }
          }
        }
      });
    }
  },

  async renderList() {
    return `
      <div class="page-header">
        <h1 class="page-title">Reports</h1>
      </div>
      <div style="display:grid; grid-template-columns: repeat(auto-fit, minmax(320px, 1fr)); gap: 20px;">
        <div class="card">
          <div class="card-header" style="border-bottom:1px solid var(--border-l);padding-bottom:12px;margin-bottom:12px">
            <span class="card-title" style="font-size:1rem">📊 Financial Statements</span>
          </div>
          <ul style="list-style:none;padding:0">
            <li style="padding:10px 0"><a href="#/reports/profit-and-loss" style="color:var(--text);font-weight:500;display:block">Profit and Loss</a><span style="font-size:.75rem;color:var(--text-s)">Income vs expenses from the ledger</span></li>
            <li style="padding:10px 0"><a href="#/reports/balance-sheet" style="color:var(--text);font-weight:500;display:block">Balance Sheet</a><span style="font-size:.75rem;color:var(--text-s)">Assets = Liabilities + Equity</span></li>
            <li style="padding:10px 0"><a href="#/reports/cash-flow" style="color:var(--text);font-weight:500;display:block">Cash Flow Statement</a><span style="font-size:.75rem;color:var(--text-s)">Money in vs money out through cash/bank</span></li>
          </ul>
        </div>
        <div class="card">
          <div class="card-header" style="border-bottom:1px solid var(--border-l);padding-bottom:12px;margin-bottom:12px">
            <span class="card-title" style="font-size:1rem">💰 Sales & Receivables</span>
          </div>
          <ul style="list-style:none;padding:0">
            <li style="padding:10px 0"><a href="#/reports/sales-by-customer" style="color:var(--text);font-weight:500">Sales by Customer</a></li>
            <li style="padding:10px 0"><a href="#/reports/aging" style="color:var(--text);font-weight:500">Aging Summary</a><span style="font-size:.75rem;color:var(--text-s);display:block">Outstanding invoices by age</span></li>
          </ul>
        </div>
        <div class="card">
          <div class="card-header" style="border-bottom:1px solid var(--border-l);padding-bottom:12px;margin-bottom:12px">
            <span class="card-title" style="font-size:1rem">📋 Accountant</span>
          </div>
          <ul style="list-style:none;padding:0">
            <li style="padding:10px 0"><a href="#/ledger" style="color:var(--text);font-weight:500">General Ledger</a></li>
            <li style="padding:10px 0"><a href="#/chart-of-accounts" style="color:var(--text);font-weight:500">Chart of Accounts</a></li>
            <li style="padding:10px 0"><a href="#/reports/trial-balance" style="color:var(--text);font-weight:500">Trial Balance</a><span style="font-size:.75rem;color:var(--text-s);display:block">Verify debits = credits</span></li>
            <li style="padding:10px 0"><a href="#/reports/expense-by-category" style="color:var(--text);font-weight:500">Expense by Category</a></li>
          </ul>
        </div>
        <div class="card">
          <div class="card-header" style="border-bottom:1px solid var(--border-l);padding-bottom:12px;margin-bottom:12px">
            <span class="card-title" style="font-size:1rem">🏛️ Tax & Compliance</span>
          </div>
          <ul style="list-style:none;padding:0">
            <li style="padding:10px 0"><a href="#/reports/gst-summary" style="color:var(--text);font-weight:500">GST Summary</a><span style="font-size:.75rem;color:var(--text-s);display:block">GSTR-1 / GSTR-3B preparation data</span></li>
          </ul>
        </div>
      </div>`;
  },

  async renderProfitAndLoss() {
    const fmt = (n) => '₹' + Number(n || 0).toLocaleString('en-IN', { minimumFractionDigits: 2 });
    let income = 0, expenses = 0;
    try {
      const stats = await API.get('/dashboard');
      income = stats.total_income || 0;
      expenses = stats.total_expenses || 0;
    } catch(e) {}
    const netProfit = income - expenses;

    return `
      <div class="page-header">
        <h1 class="page-title">Profit and Loss</h1>
        <div class="page-actions"><a href="#/reports" class="btn btn-secondary">Back to Reports</a></div>
      </div>
      <div class="card" style="max-width:800px;margin:0 auto">
        <div style="text-align:center;margin-bottom:30px">
          <h2 style="margin-bottom:4px">Q Manage</h2>
          <h3 style="color:var(--text-s);font-weight:500">Profit and Loss Statement</h3>
        </div>
        <table style="width:100%;border-collapse:collapse;font-size:.9rem">
          <tr><td colspan="2" style="font-weight:700;padding:12px 0;color:var(--primary)">Operating Income</td></tr>
          <tr><td style="padding:8px 0 8px 20px">Sales Revenue</td><td style="text-align:right">${fmt(income)}</td></tr>
          <tr style="border-top:1px solid var(--border-l);border-bottom:2px solid var(--border);font-weight:600"><td style="padding:10px 0">Total Income</td><td style="text-align:right">${fmt(income)}</td></tr>
          <tr><td colspan="2" style="font-weight:700;padding:20px 0 10px;color:var(--primary)">Operating Expenses</td></tr>
          <tr><td style="padding:8px 0 8px 20px">Total Expenses</td><td style="text-align:right">${fmt(expenses)}</td></tr>
          <tr style="border-top:1px solid var(--border-l);border-bottom:2px solid var(--border);font-weight:600"><td style="padding:10px 0">Total Operating Expenses</td><td style="text-align:right">${fmt(expenses)}</td></tr>
          <tr style="background:var(--bg);font-weight:700;font-size:1.1rem;color:${netProfit >= 0 ? 'var(--green)' : 'var(--red)'}"><td style="padding:16px 10px">Net Profit</td><td style="text-align:right;padding:16px 10px">${fmt(netProfit)}</td></tr>
        </table>
      </div>`;
  },

  async renderBalanceSheet() {
    const fmt = (n) => '₹' + Number(n || 0).toLocaleString('en-IN', { minimumFractionDigits: 2 });
    let data = { assets: [], liabilities: [], equity: [], totals: {} };
    try { data = await API.get('/reports/balance-sheet'); } catch(e) {}

    const renderSection = (items, color) => items.map(a =>
      `<tr><td style="padding:6px 0 6px 20px">${a.name}</td><td style="text-align:right">${fmt(a.balance)}</td></tr>`
    ).join('');

    return `
      <div class="page-header">
        <h1 class="page-title">Balance Sheet</h1>
        <div class="page-actions"><a href="#/reports" class="btn btn-secondary">Back</a></div>
      </div>
      <div class="card" style="max-width:800px;margin:0 auto">
        <div style="text-align:center;margin-bottom:24px"><h2>Balance Sheet</h2><div style="font-size:.8rem;color:var(--text-xs)">As of ${new Date().toLocaleDateString('en-IN')}</div></div>
        <table style="width:100%;border-collapse:collapse;font-size:.9rem">
          <tr><td colspan="2" style="font-weight:700;padding:12px 0;color:var(--primary)">Assets</td></tr>
          ${renderSection(data.assets || [])}
          <tr style="border-top:2px solid var(--border);font-weight:700"><td style="padding:10px 0">Total Assets</td><td style="text-align:right">${fmt(data.totals?.total_assets)}</td></tr>
          <tr><td colspan="2" style="font-weight:700;padding:20px 0 12px;color:var(--primary)">Liabilities</td></tr>
          ${renderSection(data.liabilities || [])}
          <tr style="border-top:1px solid var(--border-l);font-weight:600"><td style="padding:8px 0">Total Liabilities</td><td style="text-align:right">${fmt(data.totals?.total_liabilities)}</td></tr>
          <tr><td colspan="2" style="font-weight:700;padding:20px 0 12px;color:var(--primary)">Equity</td></tr>
          ${renderSection(data.equity || [])}
          <tr style="border-top:2px solid var(--border);font-weight:700;font-size:1.05rem"><td style="padding:12px 0">Total Liabilities + Equity</td><td style="text-align:right">${fmt((data.totals?.total_liabilities || 0) + (data.totals?.total_equity || 0))}</td></tr>
        </table>
      </div>`;
  },

  async renderTrialBalance() {
    const fmt = (n) => Number(n || 0) !== 0 ? '₹' + Number(n || 0).toLocaleString('en-IN', { minimumFractionDigits: 2 }) : '';
    let data = { accounts: [], verification: {} };
    try { data = await API.get('/reports/trial-balance'); } catch(e) {}

    const diff = data.verification?.difference || 0;
    return `
      <div class="page-header">
        <h1 class="page-title">Trial Balance</h1>
        <div class="page-actions"><a href="#/reports" class="btn btn-secondary">Back</a></div>
      </div>
      <div class="card" style="max-width:900px;margin:0 auto">
        <div style="margin-bottom:16px;padding:12px;border-radius:var(--radius);background:${diff === 0 ? 'var(--green-l)' : 'var(--red-l)'}">
          <span style="font-weight:600;font-size:.85rem">${diff === 0 ? '✅ Trial Balance is BALANCED — Debits = Credits' : '⚠️ Trial Balance is UNBALANCED — Difference: ' + fmt(diff)}</span>
        </div>
        <table class="data-table">
          <thead><tr><th>Code</th><th>Account</th><th style="text-align:right">Debit</th><th style="text-align:right">Credit</th></tr></thead>
          <tbody>
            ${(data.accounts || []).map(a => `
              <tr>
                <td style="font-family:monospace;font-size:.75rem">${a.code || ''}</td>
                <td style="font-weight:500">${a.name}</td>
                <td style="text-align:right;color:var(--red)">${fmt(a.total_debit)}</td>
                <td style="text-align:right;color:var(--green)">${fmt(a.total_credit)}</td>
              </tr>
            `).join('')}
          </tbody>
          <tfoot>
            <tr style="font-weight:700;border-top:2px solid var(--border)">
              <td colspan="2" style="padding:10px 16px">TOTAL</td>
              <td style="text-align:right;padding:10px 16px">${fmt(data.verification?.total_debit)}</td>
              <td style="text-align:right;padding:10px 16px">${fmt(data.verification?.total_credit)}</td>
            </tr>
          </tfoot>
        </table>
      </div>`;
  },

  async renderCashFlow() {
    const fmt = (n) => '₹' + Number(n || 0).toLocaleString('en-IN', { minimumFractionDigits: 2 });
    let data = { movements: [], summary: {} };
    try { data = await API.get('/reports/cash-flow'); } catch(e) {}
    const dt = (ts) => ts ? new Date(ts * 1000).toLocaleDateString('en-IN', { day: '2-digit', month: 'short' }) : '-';

    return `
      <div class="page-header">
        <h1 class="page-title">Cash Flow Statement</h1>
        <div class="page-actions"><a href="#/reports" class="btn btn-secondary">Back</a></div>
      </div>
      <div class="stats-grid">
        <div class="stat-card green"><span class="stat-label">INFLOW</span><span class="stat-value">${fmt(data.summary?.total_inflow)}</span></div>
        <div class="stat-card red"><span class="stat-label">OUTFLOW</span><span class="stat-value">${fmt(data.summary?.total_outflow)}</span></div>
        <div class="stat-card blue"><span class="stat-label">NET CASH</span><span class="stat-value" style="color:${(data.summary?.net_cash || 0) >= 0 ? 'var(--green)' : 'var(--red)'}">${fmt(data.summary?.net_cash)}</span></div>
      </div>
      <div class="table-container">
        <table class="data-table">
          <thead><tr><th>Date</th><th>Description</th><th style="text-align:right">Inflow</th><th style="text-align:right">Outflow</th></tr></thead>
          <tbody>
            ${(data.movements || []).map(m => `
              <tr>
                <td>${dt(m.entry_date)}</td>
                <td style="font-weight:500">${m.memo || '-'}</td>
                <td style="text-align:right;color:var(--green)">${m.debit > 0 ? fmt(m.debit) : ''}</td>
                <td style="text-align:right;color:var(--red)">${m.credit > 0 ? fmt(m.credit) : ''}</td>
              </tr>
            `).join('')}
          </tbody>
        </table>
      </div>`;
  },

  async renderAging() {
    const fmt = (n) => '₹' + Number(n || 0).toLocaleString('en-IN', { minimumFractionDigits: 2 });
    let data = { invoices: [], summary: {} };
    try { data = await API.get('/reports/aging'); } catch(e) {}

    return `
      <div class="page-header">
        <h1 class="page-title">Aging Summary</h1>
        <div class="page-actions"><a href="#/reports" class="btn btn-secondary">Back</a></div>
      </div>
      <div class="stats-grid">
        <div class="stat-card green"><span class="stat-label">CURRENT</span><span class="stat-value">${fmt(data.summary?.current_amt)}</span></div>
        <div class="stat-card yellow"><span class="stat-label">31-60 DAYS</span><span class="stat-value">${fmt(data.summary?.days_31_60)}</span></div>
        <div class="stat-card red"><span class="stat-label">61-90 DAYS</span><span class="stat-value">${fmt(data.summary?.days_61_90)}</span></div>
        <div class="stat-card red"><span class="stat-label">90+ DAYS</span><span class="stat-value">${fmt(data.summary?.days_90_plus)}</span></div>
      </div>
      <div class="table-container">
        ${(data.invoices || []).length ? `
        <table class="data-table">
          <thead><tr><th>Invoice</th><th>Customer</th><th>Bucket</th><th style="text-align:right">Total</th><th style="text-align:right">Balance Due</th></tr></thead>
          <tbody>
            ${(data.invoices || []).map(i => `
              <tr>
                <td style="font-weight:600;color:var(--primary)">${i.invoice_num}</td>
                <td>${i.customer || '-'}</td>
                <td><span class="badge badge-${i.aging_bucket === 'current' ? 'paid' : 'overdue'}">${i.aging_bucket}</span></td>
                <td style="text-align:right">${fmt(i.total)}</td>
                <td style="text-align:right;font-weight:600;color:var(--red)">${fmt(i.balance_due)}</td>
              </tr>
            `).join('')}
          </tbody>
        </table>` : '<div class="empty-state" style="padding:40px"><h3>No outstanding invoices</h3></div>'}
      </div>`;
  },

  async renderSalesByCustomer() {
    const fmt = (n) => '₹' + Number(n || 0).toLocaleString('en-IN', { minimumFractionDigits: 2 });
    let data = [];
    try { data = await API.get('/reports/sales-by-customer'); } catch(e) {}
    this._salesData = data;

    return `
      <div class="page-header">
        <h1 class="page-title">Sales by Customer</h1>
        <div class="page-actions"><a href="#/reports" class="btn btn-secondary">Back</a></div>
      </div>
      ${data.length > 0 ? `
      <div class="card" style="margin-bottom:20px">
        <div class="card-header"><span class="card-title">Top Customers by Revenue</span></div>
        <div style="position:relative;height:${Math.min(data.length, 8) * 36 + 40}px;padding:8px">
          <canvas id="qm-sales-chart"></canvas>
        </div>
      </div>` : ''}
      <div class="table-container">
        <table class="data-table">
          <thead><tr><th>Customer</th><th>Invoices</th><th style="text-align:right">Total Sales</th><th style="text-align:right">Paid</th><th style="text-align:right">Outstanding</th></tr></thead>
          <tbody>
            ${data.map(c => `
              <tr>
                <td style="font-weight:600">${c.customer}</td>
                <td>${c.invoice_count}</td>
                <td style="text-align:right;font-weight:500">${fmt(c.total_sales)}</td>
                <td style="text-align:right;color:var(--green)">${fmt(c.total_paid)}</td>
                <td style="text-align:right;color:var(--red);font-weight:600">${fmt(c.total_outstanding)}</td>
              </tr>
            `).join('')}
          </tbody>
        </table>
      </div>`;
  },

  async renderExpenseByCategory() {
    const fmt = (n) => '₹' + Number(n || 0).toLocaleString('en-IN', { minimumFractionDigits: 2 });
    let data = [];
    try { data = await API.get('/reports/expense-by-category'); } catch(e) {}
    this._expenseData = data;

    return `
      <div class="page-header">
        <h1 class="page-title">Expense by Category</h1>
        <div class="page-actions"><a href="#/reports" class="btn btn-secondary">Back</a></div>
      </div>
      ${data.length > 0 ? `
      <div class="card" style="margin-bottom:20px">
        <div class="card-header"><span class="card-title">Expense Breakdown</span></div>
        <div style="position:relative;height:240px;padding:12px">
          <canvas id="qm-expense-chart"></canvas>
        </div>
      </div>` : ''}
      <div class="table-container">
        <table class="data-table">
          <thead><tr><th>Category</th><th>Code</th><th>Entries</th><th style="text-align:right">Total Amount</th></tr></thead>
          <tbody>
            ${data.map(e => `
              <tr>
                <td style="font-weight:600">${e.category}</td>
                <td style="font-family:monospace;font-size:.75rem">${e.code || ''}</td>
                <td>${e.entry_count}</td>
                <td style="text-align:right;font-weight:600;color:var(--red)">${fmt(e.total_amount)}</td>
              </tr>
            `).join('')}
          </tbody>
        </table>
      </div>`;
  },

  async renderGSTSummary() {
    const fmt = (n) => '₹' + Number(n || 0).toLocaleString('en-IN', { minimumFractionDigits: 2 });
    let data = { outward_supplies: [], inward_supplies: [], tax_summary: {} };
    try { data = await API.get('/reports/gst-summary'); } catch(e) {}

    const outputTax = data.tax_summary?.output_tax || 0;
    const inputTax = data.tax_summary?.input_tax || 0;
    const netPayable = outputTax - inputTax;

    return `
      <div class="page-header">
        <h1 class="page-title">GST Summary</h1>
        <div class="page-actions"><a href="#/reports" class="btn btn-secondary">Back</a></div>
      </div>
      <div class="stats-grid">
        <div class="stat-card blue"><span class="stat-label">OUTPUT TAX (GSTR-1)</span><span class="stat-value">${fmt(outputTax)}</span></div>
        <div class="stat-card green"><span class="stat-label">INPUT TAX CREDIT</span><span class="stat-value">${fmt(inputTax)}</span></div>
        <div class="stat-card ${netPayable > 0 ? 'red' : 'green'}"><span class="stat-label">NET TAX PAYABLE</span><span class="stat-value">${fmt(netPayable)}</span></div>
      </div>
      <div class="card" style="margin-bottom:24px">
        <div class="card-header"><span class="card-title">Outward Supplies (Sales)</span></div>
        ${(data.outward_supplies || []).length ? `
        <table class="data-table">
          <thead><tr><th>Invoice</th><th>Customer</th><th>GSTIN</th><th style="text-align:right">Taxable</th><th style="text-align:right">Tax</th><th style="text-align:right">Total</th></tr></thead>
          <tbody>
            ${(data.outward_supplies || []).map(s => `
              <tr>
                <td style="font-weight:500">${s.invoice_num}</td>
                <td>${s.customer || '-'}</td>
                <td style="font-family:monospace;font-size:.72rem">${s.customer_gstin || 'URD'}</td>
                <td style="text-align:right">${fmt(s.subtotal)}</td>
                <td style="text-align:right">${fmt(s.tax)}</td>
                <td style="text-align:right;font-weight:600">${fmt(s.total)}</td>
              </tr>
            `).join('')}
          </tbody>
        </table>` : '<p style="color:var(--text-xs);font-size:.82rem">No outward supplies</p>'}
      </div>`;
  }
};
