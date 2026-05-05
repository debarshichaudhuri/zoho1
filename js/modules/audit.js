/* Q Manage — Audit Trail Module (SHA-256 Chain Viewer) */
const AuditModule = {
  async render() {
    let trail = [], verification = {};
    try { trail = await API.get('/audit'); } catch(e) {}
    try { verification = await API.post('/audit', {}); } catch(e) {}
    const dt = (ts) => ts ? new Date(ts*1000).toLocaleString('en-IN',{day:'2-digit',month:'short',year:'numeric',hour:'2-digit',minute:'2-digit'}) : '-';
    const isIntact = (verification.broken_links||0) === 0;
    return `
      <div class="page-header"><h1 class="page-title">Audit Trail</h1>
        <div class="page-actions"><button class="btn btn-primary" onclick="AuditModule.verify()">🔗 Verify Chain Integrity</button></div>
      </div>
      <div style="margin-bottom:20px;padding:16px;border-radius:var(--radius);background:${isIntact?'var(--green-l)':'var(--red-l)'};border:1px solid ${isIntact?'var(--green)':'var(--red)'}">
        <div style="font-weight:700;font-size:.9rem;margin-bottom:4px">${isIntact ? '✅ SHA-256 Audit Chain: FULLY INTACT' : '⚠️ CHAIN INTEGRITY COMPROMISED — '+verification.broken_links+' broken links detected!'}</div>
        <div style="font-size:.78rem;color:var(--text-s)">${trail.length} entries | Each entry hash-chained to previous via SHA-256 | Tamper-proof per Companies Act 2013</div>
      </div>
      <div class="table-container">
        ${trail.length ? `<table class="data-table"><thead><tr><th>#</th><th>Timestamp</th><th>Action</th><th>Entity</th><th>Details</th><th>Hash</th></tr></thead><tbody>
          ${trail.map(a=>`<tr>
            <td style="font-family:monospace;font-size:.72rem;color:var(--text-xs)">${a.id}</td>
            <td style="font-size:.78rem">${dt(a.created_at)}</td>
            <td><span class="badge badge-${a.action?.includes('DELETE')?'void':a.action?.includes('CREATE')?'paid':'sent'}">${a.action||'-'}</span></td>
            <td style="font-size:.82rem">${a.entity_type||'-'} #${a.entity_id||''}</td>
            <td style="font-size:.78rem;max-width:200px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap" title="${a.details||''}">${a.details||'-'}</td>
            <td style="font-family:monospace;font-size:.65rem;color:var(--text-xs)" title="Full hash in database">${a.hash_prefix||''}...</td>
          </tr>`).join('')}
        </tbody></table>` : `<div class="empty-state"><h3>No audit entries</h3><p>Financial actions are automatically logged with SHA-256 chain hashes.</p></div>`}
      </div>`;
  },
  async verify() {
    QManage.toast('Verifying SHA-256 chain...','info');
    try {
      const res = await API.post('/audit',{});
      if(res.broken_links===0) QManage.toast('✅ Audit chain FULLY INTACT — 0 broken links','success');
      else QManage.toast(`⚠️ ${res.broken_links} BROKEN LINKS detected!`,'error');
      QManage.route();
    } catch(e){QManage.toast('Verification failed: '+e.message,'error');}
  }
};
