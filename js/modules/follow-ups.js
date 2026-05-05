/* Q Manage — Follow-Ups & Activities Module */
window.FollowUpsModule = {
  async renderList() {
    let activities = [];
    try {
      const resp = await API.get('/activities');
      activities = resp.activities || [];
    } catch (e) {
      return `<div class="alert alert-danger">Failed to load activities: ${e.message}</div>`;
    }

    const dt = (ts) => {
      if (!ts) return '-';
      const d = new Date(ts * 1000);
      return d.toLocaleDateString('en-IN', { day: '2-digit', month: 'short', year: 'numeric' });
    };
    const typeIcon = { call: '📞', email: '✉️', meeting: '🤝', note: '📝', task: '✅' };
    const now = Math.floor(Date.now() / 1000);

    return `
      <div class="page-header">
        <h1 class="page-title">Follow-Ups & Activities</h1>
        <button class="btn btn-primary" onclick="FollowUpsModule.renderForm()">+ New Activity</button>
      </div>

      <div class="card">
        ${activities.length === 0 ? `
          <div class="empty-state" style="padding:40px">
            <p>No activities yet. Log a call, meeting, or task.</p>
          </div>
        ` : `
        <table class="data-table">
          <thead>
            <tr>
              <th>Type</th>
              <th>Subject</th>
              <th>Contact / Deal</th>
              <th>Date</th>
              <th>Status</th>
              <th>Actions</th>
            </tr>
          </thead>
          <tbody>
            ${activities.map(a => {
              const isOverdue = !a.completed && a.date < now;
              return `
                <tr>
                  <td style="font-size:1.1rem">${typeIcon[a.type] || '📋'} <span style="font-size:.75rem;text-transform:uppercase;color:var(--text-s)">${a.type}</span></td>
                  <td style="font-weight:500">${a.subject || '(no subject)'}</td>
                  <td>${a.contact_name || (a.deal_id ? 'Deal #' + a.deal_id : '-')}</td>
                  <td style="${isOverdue ? 'color:var(--red);font-weight:600' : ''}">${dt(a.date)}${isOverdue ? ' <span class="badge" style="background:var(--red-l);color:var(--red);font-size:.65rem">OVERDUE</span>' : ''}</td>
                  <td>
                    <span class="badge" style="background:${a.completed ? 'var(--green-l)' : 'var(--orange-l)'};color:${a.completed ? 'var(--green)' : 'var(--orange)'}">
                      ${a.completed ? 'DONE' : 'PENDING'}
                    </span>
                  </td>
                  <td style="display:flex;gap:6px">
                    ${!a.completed ? `<button class="btn btn-secondary" style="padding:3px 8px;font-size:.75rem" onclick="FollowUpsModule.markComplete(${a.id})">Complete</button>` : ''}
                    <button class="btn btn-secondary" style="padding:3px 8px;font-size:.75rem;color:var(--red);border-color:var(--red)" onclick="FollowUpsModule.deleteActivity(${a.id})">Delete</button>
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
        <h1 class="page-title">New Activity</h1>
        <button class="btn btn-secondary" onclick="window.location.hash='#/follow-ups'">Back</button>
      </div>
      <div class="card" style="max-width:600px">
        <div class="form-group">
          <label class="form-label required">Type</label>
          <select id="fu-type" class="form-control">
            <option value="call">📞 Call</option>
            <option value="email">✉️ Email</option>
            <option value="meeting">🤝 Meeting</option>
            <option value="task">✅ Task</option>
            <option value="note">📝 Note</option>
          </select>
        </div>
        <div class="form-group">
          <label class="form-label required">Subject</label>
          <input type="text" id="fu-subject" class="form-control" placeholder="e.g. Follow up on proposal">
        </div>
        <div class="form-group">
          <label class="form-label">Description</label>
          <textarea id="fu-desc" class="form-control" rows="3" placeholder="Notes or details..."></textarea>
        </div>
        <div class="form-group">
          <label class="form-label required">Date</label>
          <input type="date" id="fu-date" class="form-control" value="${new Date().toISOString().slice(0,10)}">
        </div>
        <div style="margin-top:24px;text-align:right">
          <button class="btn btn-primary" onclick="FollowUpsModule.save()">Save Activity</button>
        </div>
      </div>
    `;
  },

  async save() {
    const type    = document.getElementById('fu-type').value;
    const subject = document.getElementById('fu-subject').value.trim();
    const desc    = document.getElementById('fu-desc').value.trim();
    const dateStr = document.getElementById('fu-date').value;

    if (!subject) { QManage.toast('Subject is required', 'error'); return; }
    if (!dateStr) { QManage.toast('Date is required', 'error'); return; }

    const date = Math.floor(new Date(dateStr).getTime() / 1000);

    try {
      await API.post('/activities', { type, subject, description: desc, date });
      QManage.toast('Activity saved', 'success');
      window.location.hash = '#/follow-ups';
    } catch (e) {
      QManage.toast('Failed to save: ' + e.message, 'error');
    }
  },

  async markComplete(id) {
    try {
      await API.put('/activities/' + id, { completed: true });
      QManage.toast('Marked as complete', 'success');
      const html = await this.renderList();
      document.getElementById('page-content').innerHTML = html;
    } catch (e) {
      QManage.toast('Error: ' + e.message, 'error');
    }
  },

  async deleteActivity(id) {
    if (!confirm('Delete this activity?')) return;
    try {
      await API.del('/activities/' + id);
      QManage.toast('Deleted', 'success');
      const html = await this.renderList();
      document.getElementById('page-content').innerHTML = html;
    } catch (e) {
      QManage.toast('Error: ' + e.message, 'error');
    }
  }
};
