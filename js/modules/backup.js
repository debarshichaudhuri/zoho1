/* Q Manage — Backup & Sync Module (Production Grade) */
const BackupModule = {
  async render() {
    let backups = [];
    try {
      backups = await API.get('/backup');
    } catch (e) { /* offline */ }

    const fmt = (bytes) => {
      if (bytes < 1024) return bytes + ' B';
      if (bytes < 1048576) return (bytes / 1024).toFixed(1) + ' KB';
      return (bytes / 1048576).toFixed(1) + ' MB';
    };

    return `
      <div class="page-header">
        <h1 class="page-title">Backup & Data Protection</h1>
        <div class="page-actions">
          <button class="btn btn-primary" onclick="BackupModule.createBackup()">
            <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="17 8 12 3 7 8"/><line x1="12" y1="3" x2="12" y2="15"/></svg>
            Create Backup Now
          </button>
        </div>
      </div>

      <div class="stats-grid" style="margin-bottom:24px">
        <div class="stat-card blue">
          <span class="stat-label">Total Backups</span>
          <span class="stat-value">${backups.length}</span>
          <span class="stat-change">Stored locally</span>
        </div>
        <div class="stat-card green">
          <span class="stat-label">Encrypted</span>
          <span class="stat-value">${backups.filter(b => b.encrypted).length}</span>
          <span class="stat-change up">AES-256 Protected</span>
        </div>
        <div class="stat-card yellow">
          <span class="stat-label">Last Backup</span>
          <span class="stat-value" style="font-size:1rem">${backups.length ? new Date(backups[0].created_at * 1000).toLocaleDateString('en-IN') : 'Never'}</span>
          <span class="stat-change">${backups.length ? new Date(backups[0].created_at * 1000).toLocaleTimeString('en-IN') : 'Create your first backup'}</span>
        </div>
      </div>

      <div class="card" style="margin-bottom:24px">
        <div class="card-header">
          <span class="card-title">Encryption Settings</span>
        </div>
        <p style="font-size:.82rem;color:var(--text-s);margin-bottom:12px">
          Set a passphrase to encrypt backups. Without a passphrase, backups are stored as plain SQLite files.
          <strong>If you lose the passphrase, encrypted backups cannot be recovered.</strong>
        </p>
        <div class="form-row" style="max-width:600px">
          <div class="form-group">
            <label class="form-label">Backup Passphrase</label>
            <input class="form-control" type="password" id="backup-passphrase" placeholder="Leave empty for unencrypted">
          </div>
          <div class="form-group" style="display:flex;align-items:flex-end">
            <button class="btn btn-secondary" onclick="BackupModule.createBackup()">Create Encrypted Backup</button>
          </div>
        </div>
      </div>

      <div class="card">
        <div class="card-header">
          <span class="card-title">Backup History</span>
          <button class="btn btn-ghost btn-sm" onclick="location.hash='#/backup';QManage.route()">Refresh</button>
        </div>
        ${backups.length ? `
        <table class="data-table">
          <thead>
            <tr><th>Date</th><th>Filename</th><th>Size</th><th>Encrypted</th><th>Action</th></tr>
          </thead>
          <tbody>
            ${backups.map(b => `
              <tr>
                <td style="font-weight:500">${new Date(b.created_at * 1000).toLocaleString('en-IN')}</td>
                <td style="font-family:monospace;font-size:.75rem;color:var(--text-s)">${(b.filename || '').split('/').pop()}</td>
                <td>${fmt(b.size_bytes || 0)}</td>
                <td>${b.encrypted ? '<span class="badge badge-paid">ENCRYPTED</span>' : '<span class="badge badge-draft">PLAIN</span>'}</td>
                <td><button class="btn btn-ghost btn-sm">Restore</button></td>
              </tr>
            `).join('')}
          </tbody>
        </table>` : `
        <div class="empty-state" style="padding:40px">
          <svg viewBox="0 0 24 24" width="48" height="48" fill="none" stroke="currentColor" stroke-width="1.5" style="opacity:.3;margin-bottom:12px"><polyline points="21 8 21 21 3 21 3 8"/><rect x="1" y="3" width="22" height="5"/><line x1="10" y1="12" x2="14" y2="12"/></svg>
          <h3>No backups yet</h3>
          <p>Create your first backup to protect your financial data.</p>
        </div>`}
      </div>

      <div class="card" style="margin-top:24px">
        <div class="card-header"><span class="card-title">Data Sovereignty</span></div>
        <div style="display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:16px;padding:4px 0">
          <div style="display:flex;align-items:flex-start;gap:10px">
            <div style="width:32px;height:32px;border-radius:8px;background:var(--green-l);color:var(--green);display:flex;align-items:center;justify-content:center;flex-shrink:0">
              <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><path d="M22 11.08V12a10 10 0 1 1-5.93-9.14"/><polyline points="22 4 12 14.01 9 11.01"/></svg>
            </div>
            <div><div style="font-weight:600;font-size:.82rem">Zero Cloud</div><div style="font-size:.72rem;color:var(--text-s)">All data stays on your device</div></div>
          </div>
          <div style="display:flex;align-items:flex-start;gap:10px">
            <div style="width:32px;height:32px;border-radius:8px;background:var(--primary-l);color:var(--primary);display:flex;align-items:center;justify-content:center;flex-shrink:0">
              <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><rect x="3" y="11" width="18" height="11" rx="2" ry="2"/><path d="M7 11V7a5 5 0 0 1 10 0v4"/></svg>
            </div>
            <div><div style="font-weight:600;font-size:.82rem">Encrypted</div><div style="font-size:.72rem;color:var(--text-s)">AES-256 passphrase protection</div></div>
          </div>
          <div style="display:flex;align-items:flex-start;gap:10px">
            <div style="width:32px;height:32px;border-radius:8px;background:var(--yellow-l);color:var(--yellow);display:flex;align-items:center;justify-content:center;flex-shrink:0">
              <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><polyline points="14 2 14 8 20 8"/></svg>
            </div>
            <div><div style="font-weight:600;font-size:.82rem">Portable</div><div style="font-size:.72rem;color:var(--text-s)">Single .qbak file, any OS</div></div>
          </div>
        </div>
      </div>
    `;
  },

  async createBackup() {
    const passEl = document.getElementById('backup-passphrase');
    const passphrase = passEl ? passEl.value : '';

    QManage.toast('Creating backup...', 'info');
    try {
      const res = await API.post('/backup', { passphrase });
      QManage.toast(`Backup created: ${(res.path || '').split('/').pop()}`, 'success');
      // Refresh the page
      QManage.route();
    } catch (e) {
      QManage.toast('Backup failed: ' + e.message, 'error');
    }
  }
};
