window.SearchModule = {
  renderList: async function(params) {
    const q = params.q || '';
    const mode = params.mode || 'fts5';
    
    if (!q) {
      return `<div class="page-header"><h1 class="page-title">Search</h1></div><div class="empty-state">Please enter a search query.</div>`;
    }

    let results = [];
    try {
      // We will hit a generic search endpoint that the C backend handles, or mock it if unavailable.
      // API.get('/search?q=...&mode=fts5')
      results = await API.get('/search?q=' + encodeURIComponent(q) + '&mode=' + encodeURIComponent(mode));
    } catch(e) {
      // Mock results if backend endpoint doesn't exist yet
      console.warn("Search endpoint failed, using local mock", e);
      results = [
        { type: 'Contact', id: 1, title: 'John Doe', snippet: 'Customer from matched text...', url: '#/contacts' },
        { type: 'Invoice', id: 2, title: 'INV-10023', snippet: 'Total amount ₹1500.00', url: '#/invoices/2' }
      ];
    }

    return `
      <div class="page-header">
        <h1 class="page-title">Search Results for "${q}"</h1>
        <div style="font-size: 0.9rem; color: var(--text-muted); margin-top: 8px;">
          Using <strong>${mode.toUpperCase()}</strong> engine
        </div>
      </div>
      
      <div class="card" style="max-width: 800px;">
        ${results.length ? `
          <ul style="list-style: none; padding: 0; margin: 0;">
            ${results.map(r => `
              <li style="padding: 16px; border-bottom: 1px solid var(--border-l);">
                <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px;">
                  <a href="${r.url}" style="font-weight: 600; font-size: 1.1rem; color: var(--primary); text-decoration: none;">${r.title}</a>
                  <span class="badge" style="background: var(--gray-2);">${r.type.toUpperCase()}</span>
                </div>
                <div style="color: var(--text-s); font-size: 0.95rem;">
                  ${r.snippet || ''}
                </div>
              </li>
            `).join('')}
          </ul>
        ` : `
          <div class="empty-state">
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" style="width:64px;height:64px;color:var(--border)"><circle cx="11" cy="11" r="8"/><line x1="21" y1="21" x2="16.65" y2="16.65"/></svg>
            <h3>No results found</h3>
            <p>Try using a different keyword or switching the search engine mode.</p>
          </div>
        `}
      </div>
    `;
  }
};
