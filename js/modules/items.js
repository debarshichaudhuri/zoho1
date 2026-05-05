/* Q Manage — Items Module (C API Integrated) */
const ItemsModule = {
  async renderList() {
    let items = [];
    try { items = await API.get('/items'); } catch(e) { /* offline */ }

    const fmt = (n) => '₹' + Number(n || 0).toLocaleString('en-IN', { minimumFractionDigits: 2 });

    return `
      <div class="page-header">
        <h1 class="page-title">Items</h1>
        <div class="page-actions">
          <button class="btn btn-primary" onclick="ItemsModule.showCreateForm()">
            <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/></svg>
            New Item
          </button>
        </div>
      </div>
      <div class="table-container">
        ${items.length ? `
        <table class="data-table">
          <thead><tr><th>Name</th><th>SKU</th><th style="text-align:right">Price</th><th>Tax Rate</th><th>Stock</th></tr></thead>
          <tbody>
            ${items.map(item => `
              <tr>
                <td style="font-weight:600;color:var(--primary)">${item.name}</td>
                <td>${item.sku || '-'}</td>
                <td style="text-align:right;font-weight:500">${fmt(item.price)}</td>
                <td>${item.tax_rate || 0}%</td>
                <td>${item.stock || 0}</td>
              </tr>
            `).join('')}
          </tbody>
        </table>` : `
        <div class="empty-state">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" style="width:64px;height:64px"><path d="M20.59 13.41l-7.17 7.17a2 2 0 01-2.83 0L2 12V2h10l8.59 8.59a2 2 0 010 2.82z"/><line x1="7" y1="7" x2="7.01" y2="7"/></svg>
          <h3>No items yet</h3>
          <p>Add products and services you sell.</p>
          <button onclick="ItemsModule.showCreateForm()" class="btn btn-primary">+ Add Item</button>
        </div>`}
      </div>`;
  },

  showCreateForm() {
    const content = document.getElementById('page-content');
    content.innerHTML = `
      <div class="page-header">
        <h1 class="page-title">New Item</h1>
        <div class="page-actions">
          <a href="#/items" class="btn btn-secondary">Cancel</a>
          <button class="btn btn-primary" onclick="ItemsModule.save()">Save Item</button>
        </div>
      </div>
      <div class="card" style="max-width:700px">
        <div class="form-section">
          <div class="form-section-title">Item Details</div>
          <div class="form-row">
            <div class="form-group">
              <label class="form-label required">Name</label>
              <input class="form-control" id="item-name" placeholder="Product or service name">
            </div>
            <div class="form-group">
              <label class="form-label">SKU</label>
              <input class="form-control" id="item-sku" placeholder="e.g. PROD-001">
            </div>
          </div>
          <div class="form-row">
            <div class="form-group">
              <label class="form-label required">Selling Price (₹)</label>
              <input class="form-control" type="number" step="0.01" id="item-price" placeholder="0.00">
            </div>
            <div class="form-group">
              <label class="form-label">Tax Rate (%)</label>
              <select class="form-control" id="item-tax">
                <option value="0">No Tax (0%)</option>
                <option value="5">GST 5%</option>
                <option value="12">GST 12%</option>
                <option value="18" selected>GST 18%</option>
                <option value="28">GST 28%</option>
              </select>
            </div>
          </div>
        </div>
      </div>`;
  },

  async save() {
    const name = document.getElementById('item-name')?.value?.trim();
    if (!name) { QManage.toast('Item name is required', 'error'); return; }

    try {
      await API.post('/items', {
        name: name,
        sku: document.getElementById('item-sku')?.value?.trim() || '',
        price: Number(document.getElementById('item-price')?.value) || 0,
        tax_rate: Number(document.getElementById('item-tax')?.value) || 18
      });
      QManage.toast(`Item "${name}" created`, 'success');
      location.hash = '#/items';
    } catch(e) {
      QManage.toast('Save failed: ' + e.message, 'error');
    }
  },

  async renderForm() { this.showCreateForm(); return ''; }
};
