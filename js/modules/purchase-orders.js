/* Q Manage — Purchase Orders Module */
const PurchaseOrdersModule = {
  async renderList() {
    let orders = [];
    try { orders = await API.get('/purchase-orders'); } catch(e) {}
    const fmt = (n) => '₹' + Number(n || 0).toLocaleString('en-IN',{minimumFractionDigits:2});
    const dt = (ts) => ts ? new Date(ts*1000).toLocaleDateString('en-IN',{day:'2-digit',month:'short',year:'numeric'}) : '-';
    const totalPO = orders.reduce((s,o) => s+(o.total||0), 0);
    return `
      <div class="page-header"><h1 class="page-title">Purchase Orders</h1>
        <div class="page-actions"><button class="btn btn-primary" onclick="PurchaseOrdersModule.showCreateForm()">+ New PO</button></div>
      </div>
      <div class="stats-grid">
        <div class="stat-card blue"><span class="stat-label">Total POs</span><span class="stat-value">${orders.length}</span></div>
        <div class="stat-card yellow"><span class="stat-label">Pending</span><span class="stat-value">${orders.filter(o=>o.status!=='received'&&o.status!=='cancelled').length}</span></div>
        <div class="stat-card red"><span class="stat-label">Total Value</span><span class="stat-value">${fmt(totalPO)}</span></div>
      </div>
      <div class="table-container">
        ${orders.length ? `<table class="data-table"><thead><tr><th>Date</th><th>PO #</th><th>Vendor</th><th>Status</th><th style="text-align:right">Total</th></tr></thead><tbody>
          ${orders.map(o=>`<tr><td>${dt(o.date)}</td><td style="font-weight:600;color:var(--primary)">${o.po_num||'-'}</td><td>${o.vendor_name||'-'}</td><td><span class="badge badge-${o.status==='received'?'paid':o.status==='cancelled'?'void':'sent'}">${(o.status||'draft').toUpperCase()}</span></td><td style="text-align:right;font-weight:600">${fmt(o.total)}</td></tr>`).join('')}
        </tbody></table>` : `<div class="empty-state"><h3>No purchase orders</h3><p>Create POs to formalize vendor procurement.</p><button onclick="PurchaseOrdersModule.showCreateForm()" class="btn btn-primary">+ New PO</button></div>`}
      </div>`;
  },
  async showCreateForm() {
    let contacts = [];
    try { contacts = (await API.get('/contacts')).filter(c=>c.type==='vendor'||c.type==='both'); } catch(e){}
    document.getElementById('page-content').innerHTML = `
      <div class="page-header"><h1 class="page-title">New Purchase Order</h1>
        <div class="page-actions"><a href="#/purchase-orders" class="btn btn-secondary">Cancel</a><button class="btn btn-primary" onclick="PurchaseOrdersModule.save()">Create PO</button></div>
      </div>
      <div class="card" style="max-width:700px"><div class="form-section">
        <div class="form-row"><div class="form-group"><label class="form-label required">PO Number</label><input class="form-control" id="po-num" value="PO-${String(Date.now()).slice(-5)}"></div>
        <div class="form-group"><label class="form-label required">Date</label><input class="form-control" type="date" id="po-date" value="${new Date().toISOString().split('T')[0]}"></div></div>
        <div class="form-row"><div class="form-group"><label class="form-label required">Vendor</label><select class="form-control" id="po-vendor"><option value="">Select</option>${contacts.map(c=>`<option value="${c.id}">${c.display_name}</option>`).join('')}</select></div>
        <div class="form-group"><label class="form-label required">Amount (₹)</label><input class="form-control" type="number" step="0.01" id="po-amount" placeholder="0.00"></div></div>
        <div class="form-group"><label class="form-label">Notes</label><textarea class="form-control" id="po-notes" placeholder="Order details..."></textarea></div>
      </div></div>`;
  },
  async save() {
    const num = document.getElementById('po-num')?.value?.trim();
    const vendor = document.getElementById('po-vendor')?.value;
    const amount = Number(document.getElementById('po-amount')?.value);
    if(!num||!vendor||amount<=0){QManage.toast('Fill required fields','error');return;}
    try {
      await API.post('/purchase-orders',{po_num:num,vendor_id:parseInt(vendor),total:amount,date:Math.floor(new Date(document.getElementById('po-date')?.value).getTime()/1000),notes:document.getElementById('po-notes')?.value||''});
      QManage.toast(`PO ${num} created`,'success'); location.hash='#/purchase-orders';
    } catch(e){QManage.toast('Failed: '+e.message,'error');}
  }
};
