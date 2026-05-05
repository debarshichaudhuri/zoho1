/* Q Manage — C Backend API Client
 * All money values are stored as paisa (integer, ×100) in the backend.
 * Use toRupees() / toPaisa() helpers when displaying or sending amounts.
 */

// ============================================================
// Money conversion helpers
// ============================================================
const Money = {
  toPaisa: (rupees) => Math.round(parseFloat(rupees || 0) * 100),
  toRupees: (paisa) => (parseInt(paisa || 0) / 100).toFixed(2),
  format: (paisa) => '₹' + (parseInt(paisa || 0) / 100).toLocaleString('en-IN', {
    minimumFractionDigits: 2, maximumFractionDigits: 2
  })
};

// ============================================================
// Session token storage
// ============================================================
const Auth = {
  getToken: () => localStorage.getItem('qmanage_token'),
  setToken: (t) => localStorage.setItem('qmanage_token', t),
  clearToken: () => localStorage.removeItem('qmanage_token'),
  isLoggedIn: () => !!localStorage.getItem('qmanage_token')
};

// ============================================================
// Server URL helpers
// ============================================================
function getDefaultServerUrl() {
  if (typeof window !== 'undefined' && window.location && window.location.origin && window.location.origin !== 'null') {
    const url = new URL(window.location.origin);
    // Always use backend on localhost/127.0.0.1, not the current origin
    if (url.hostname === 'localhost' || url.hostname === '127.0.0.1') {
      return 'http://localhost:9741/api';
    }
    return `${window.location.origin}/api`;
  }
  return 'http://localhost:9741/api';
}

function normalizeServerUrl(url) {
  const fallback = getDefaultServerUrl();
  const raw = (url || '').trim();
  if (!raw) return fallback;

  try {
    const resolved = new URL(raw, fallback);
    const browserHost = typeof window !== 'undefined' && window.location ? window.location.hostname.toLowerCase() : '';
    const resolvedHost = resolved.hostname.toLowerCase();
    const browserIsLocal = ['localhost', '127.0.0.1', '::1'].includes(browserHost);
    const resolvedIsLocal = ['localhost', '127.0.0.1', '::1'].includes(resolvedHost);

    if (resolvedIsLocal && browserHost && !browserIsLocal) {
      return fallback;
    }

    resolved.pathname = resolved.pathname.replace(/\/+$/, '');
    if (!resolved.pathname || resolved.pathname === '/') {
      resolved.pathname = '/api';
    }

    return resolved.toString().replace(/\/$/, '');
  } catch {
    return raw.replace(/\/+$/, '');
  }
}

// ============================================================
// API Client
// ============================================================
const API = {
  BASE_URL: (() => {
    const isLocalhost = typeof window !== 'undefined' &&
      ['localhost', '127.0.0.1'].includes(window.location.hostname);
    // Ignore localStorage on localhost - always use computed default
    const baseUrl = isLocalhost ? getDefaultServerUrl() :
      normalizeServerUrl(localStorage.getItem('qmanage_server') || getDefaultServerUrl());
    console.log('API BASE_URL initialized:', baseUrl);
    return baseUrl;
  })(),

  setServer(url) {
    const normalized = normalizeServerUrl(url);
    localStorage.setItem('qmanage_server', normalized);
    this.BASE_URL = normalized;
    return normalized;
  },

  _headers(extra = {}) {
    const h = { 'Content-Type': 'application/json', ...extra };
    const token = Auth.getToken();
    if (token) h['X-Auth-Token'] = token;
    return h;
  },

  async checkHealth() {
    try {
      const res = await fetch(`${this.BASE_URL}/dashboard`, {
        headers: this._headers()
      });
      this.isOnline = res.ok || res.status === 401; // 401 = server up but needs auth
      return this.isOnline;
    } catch (e) {
      this.isOnline = false;
      return false;
    }
  },

  async login(username, password) {
    console.log('Login attempt:', { username, baseUrl: this.BASE_URL });
    try {
      const res = await fetch(`${this.BASE_URL}/auth/login`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ username, password })
      });
      console.log('Login response status:', res.status);
      if (!res.ok) {
        const errText = await res.text();
        console.log('Login error response:', errText);
        const err = JSON.parse(errText);
        throw new Error(err.error || `Login failed (${res.status})`);
      }
      const data = await res.json();
      console.log('Login success, token received:', data.token ? 'yes' : 'no');
      Auth.setToken(data.token);
      return data;
    } catch (e) {
      console.error('Login exception:', e.message);
      throw e;
    }
  },

  async logout() {
    await this.post('/auth/logout', {}).catch(() => { });
    Auth.clearToken();
  },

  async get(endpoint, params = {}) {
    const qs = Object.keys(params).length
      ? '?' + new URLSearchParams(params).toString()
      : '';
    const res = await fetch(`${this.BASE_URL}${endpoint}${qs}`, {
      headers: this._headers()
    });
    if (res.status === 401) { Auth.clearToken(); window.location.reload(); return; }
    if (!res.ok) throw new Error(`API Error: ${res.statusText}`);
    return res.json();
  },

  async post(endpoint, data, headers = {}) {
    const isRaw = typeof data === 'string';
    const h = isRaw ? { ...this._headers(), ...headers } : this._headers(headers);
    if (isRaw) delete h['Content-Type'];

    const res = await fetch(`${this.BASE_URL}${endpoint}`, {
      method: 'POST',
      headers: h,
      body: isRaw ? data : JSON.stringify(data)
    });
    if (res.status === 401) { Auth.clearToken(); window.location.reload(); return; }
    if (!res.ok) {
      const err = await res.json().catch(() => ({}));
      throw new Error(err.error || `API Error: ${res.statusText}`);
    }
    return res.json();
  },

  async put(endpoint, data) {
    const res = await fetch(`${this.BASE_URL}${endpoint}`, {
      method: 'PUT',
      headers: this._headers(),
      body: JSON.stringify(data)
    });
    if (res.status === 401) { Auth.clearToken(); window.location.reload(); return; }
    if (!res.ok) throw new Error(`API Error: ${res.statusText}`);
    return res.json();
  },

  async del(endpoint) {
    const res = await fetch(`${this.BASE_URL}${endpoint}`, {
      method: 'DELETE',
      headers: this._headers()
    });
    if (res.status === 401) { Auth.clearToken(); window.location.reload(); return; }
    if (!res.ok) throw new Error(`API Error: ${res.statusText}`);
    return res.json();
  },

  // Paginated list helper
  async list(endpoint, page = 1, pageSize = 50) {
    return this.get(endpoint, { limit: pageSize, offset: (page - 1) * pageSize });
  }
};
