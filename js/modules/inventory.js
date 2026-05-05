/* Q Manage — Inventory Management Module */
const InventoryModule = {
  async renderList() {
    let items = [];
    try { items = await API.get('/inventory'); } catch(e) {}
    const fmt = (n) => '₹' + Number(n || 0).toLocaleString('en-IN',{minimumFractionDigits:2});
    const lowStock = items.filter(i => (i.stock_quantity||0) <= (i.reorder_level||5));
    return `
      <div class="page-header"><h1 class="page-title">Inventory</h1>
        <div class="page-actions"><button class="btn btn-primary" onclick="InventoryModule.showMovementForm()">+ Stock Movement</button></div>
      </div>
      <div class="stats-grid">
        <div class="stat-card blue"><span class="stat-label">Total SKUs</span><span class="stat-value">${items.length}</span></div>
        <div class="stat-card red"><span class="stat-label">Low Stock</span><span class="stat-value">${lowStock.length}</span></div>
        <div class="stat-card green"><span class="stat-label">Total Value</span><span class="stat-value">${fmt(items.reduce((s,i)=>s+((i.stock_quantity||0)*(i.price||0)),0))}</span></div>
      </div>
      <div class="table-container">
        ${items.length ? `<table class="data-table"><thead><tr><th>Item</th><th>SKU</th><th>Warehouse</th><th>Stock</th><th>Reorder</th><th style="text-align:right">Value</th></tr></thead><tbody>
          ${items.map(i=>`<tr>
            <td style="font-weight:600">${i.name||'-'}</td>
            <td style="font-family:monospace;font-size:.75rem">${i.sku||'-'}</td>
            <td>${i.warehouse_name||'Default'}</td>
            <td style="font-weight:600;color:${(i.stock_quantity||0)<=(i.reorder_level||5)?'var(--red)':'var(--green)'}">${i.stock_quantity||0} ${i.unit||'pcs'}</td>
            <td>${i.reorder_level||5}</td>
            <td style="text-align:right;font-weight:500">${fmt((i.stock_quantity||0)*(i.price||0))}</td>
          </tr>`).join('')}
        </tbody></table>` : `<div class="empty-state"><h3>No inventory items</h3><p>Add items and track stock levels across warehouses.</p><a href="#/items" class="btn btn-primary">Go to Items</a></div>`}
      </div>`;
  },
  async showMovementForm() {
    let items = [];
    try { items = await API.get('/items'); } catch(e){}
    document.getElementById('page-content').innerHTML = `
      <div class="page-header"><h1 class="page-title">Stock Movement</h1>
        <div class="page-actions"><a href="#/inventory" class="btn btn-secondary">Cancel</a><button class="btn btn-primary" onclick="InventoryModule.recordMovement()">Record</button></div>
      </div>
      <div class="card" style="max-width:600px"><div class="form-section">
        <div class="form-group"><label class="form-label required">Item</label><select class="form-control" id="inv-item"><option value="">Select Item</option>${items.map(i=>`<option value="${i.id}">${i.name} (${i.sku||'N/A'})</option>`).join('')}</select></div>
        <div class="form-row">
          <div class="form-group"><label class="form-label required">Type</label><select class="form-control" id="inv-type"><option value="IN">Stock In (Purchase)</option><option value="OUT">Stock Out (Sale)</option><option value="ADJUST">Adjustment</option></select></div>
          <div class="form-group"><label class="form-label required">Quantity</label><input class="form-control" type="number" id="inv-qty" value="1" min="1"></div>
        </div>
        <div class="form-group"><label class="form-label">Reference / Notes</label><input class="form-control" id="inv-ref" placeholder="PO-12345 or adjustment reason"></div>
      </div></div>`;
  },
  async recordMovement() {
    const item=document.getElementById('inv-item')?.value, qty=Number(document.getElementById('inv-qty')?.value);
    if(!item||qty<=0){QManage.toast('Select item and quantity','error');return;}
    try {
      await API.post('/inventory',{item_id:parseInt(item),type:document.getElementById('inv-type')?.value||'IN',quantity:qty,reference:document.getElementById('inv-ref')?.value||''});
      QManage.toast('Stock movement recorded','success'); location.hash='#/inventory';
    } catch(e){QManage.toast('Failed: '+e.message,'error');}
  }
};
