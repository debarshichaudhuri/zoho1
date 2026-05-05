/* Q Manage — Settings Module */
const SettingsModule = {
  _activeTab: 'org',

  async render() {
    return `
      <div class="page-header">
        <h1 class="page-title">Settings</h1>
      </div>

      <div style="display:flex; gap: 24px; max-width: 1000px">
        <!-- Settings Sidebar -->
        <div style="width: 240px; flex-shrink: 0;">
          <div class="card" style="padding: 0; overflow: hidden;">
            <ul style="list-style:none; margin:0; padding:0;">
              <li><a href="#" class="nav-link settings-tab-link" data-tab="org" style="padding: 12px 16px; background: var(--primary-l); color: var(--primary); font-weight: 600; display: block; border-left: 3px solid var(--primary);">Organization Profile</a></li>
              <li><a href="#" class="nav-link settings-tab-link" data-tab="gst" style="padding: 12px 16px; display: block; color: var(--text);">Taxes & GST</a></li>
              <li><a href="#" class="nav-link settings-tab-link" data-tab="prefs" style="padding: 12px 16px; display: block; color: var(--text);">Preferences</a></li>
              <li><a href="#" class="nav-link settings-tab-link" data-tab="migration" style="padding: 12px 16px; display: block; color: var(--text);">Data Migration</a></li>
              <li><a href="#" class="nav-link settings-tab-link" data-tab="users" style="padding: 12px 16px; display: block; color: var(--text);">Users & Roles</a></li>
              <li><a href="#" class="nav-link settings-tab-link" data-tab="reminders" style="padding: 12px 16px; display: block; color: var(--text);">Reminders</a></li>
              <li><a href="#" class="nav-link settings-tab-link" data-tab="ai" style="padding: 12px 16px; display: block; color: var(--text);">
                AI Assistant
                <span style="margin-left:6px;font-size:.65rem;background:var(--primary);color:#fff;padding:1px 6px;border-radius:10px;vertical-align:middle">NEW</span>
              </a></li>
            </ul>
          </div>
        </div>

        <!-- Settings Content -->
        <div style="flex: 1;">
          <div class="card">
            <div class="card-header"><span class="card-title">Organization Profile</span></div>
            
            <div style="display:flex; gap: 24px; margin-bottom: 24px;">
              <div style="width: 100px; height: 100px; border-radius: 8px; border: 1px dashed var(--border); display: flex; align-items: center; justify-content: center; color: var(--text-s); cursor: pointer; background: var(--bg);">
                <div style="text-align: center;">
                  <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="17 8 12 3 7 8"/><line x1="12" y1="3" x2="12" y2="15"/></svg>
                  <div style="font-size: 0.7rem; margin-top: 4px;">Upload Logo</div>
                </div>
              </div>
              <div style="flex: 1;">
                <div class="form-group">
                  <label class="form-label required">Organization Name</label>
                  <input class="form-control" value="Q Manage Demo Org">
                </div>
                <div class="form-group">
                  <label class="form-label required">Industry</label>
                  <select class="form-control">
                    <option>Software / IT Services</option>
                    <option>Retail / Ecommerce</option>
                    <option>Manufacturing</option>
                    <option>Consulting</option>
                  </select>
                </div>
              </div>
            </div>

            <div class="form-section-title">Company Address</div>
            <div class="form-group">
              <label class="form-label">Address Line 1</label>
              <input class="form-control">
            </div>
            <div class="form-row">
              <div class="form-group">
                <label class="form-label">City</label>
                <input class="form-control">
              </div>
              <div class="form-group">
                <label class="form-label">State / Province</label>
                <input class="form-control">
              </div>
            </div>
            <div class="form-row">
              <div class="form-group">
                <label class="form-label">Zip / Postal Code</label>
                <input class="form-control">
              </div>
              <div class="form-group">
                <label class="form-label">Country</label>
                <select class="form-control"><option>India</option></select>
              </div>
            </div>

            <div class="form-section-title" style="margin-top: 24px">Regional Settings</div>
            <div class="form-row">
              <div class="form-group">
                <label class="form-label required">Base Currency</label>
                <select class="form-control" disabled><option>INR - Indian Rupee</option></select>
              </div>
              <div class="form-group">
                <label class="form-label required">Fiscal Year</label>
                <select class="form-control">
                  <option>April - March</option>
                  <option>January - December</option>
                </select>
              </div>
            </div>
            <div class="form-row">
              <div class="form-group">
                <label class="form-label required">Time Zone</label>
                <select class="form-control"><option>(GMT+05:30) India Standard Time</option></select>
              </div>
              <div class="form-group">
                <label class="form-label required">Date Format</label>
                <select class="form-control"><option>dd/MM/yyyy</option></select>
              </div>
            </div>

            <div style="margin-top: 30px; padding-top: 20px; border-top: 1px solid var(--border-l); text-align: right;">
              <button class="btn btn-primary" onclick="QManage.toast('Settings saved', 'success')">Save</button>
            </div>
          </div>

          <!-- Migration Panel -->
          <div class="card" style="margin-top: 24px;">
            <div class="card-header"><span class="card-title">Data Migration (Zero-API Import)</span></div>
            <p style="color:var(--text-s); font-size: 0.9rem; margin-bottom: 24px;">Import your data from Zoho Books or other accounting software directly using CSV files. This process runs entirely locally using the high-speed C backend.</p>

            <div style="border: 1px solid var(--border); border-radius: 8px; padding: 20px; margin-bottom: 16px;">
              <h3 style="font-size: 1rem; margin-bottom: 8px;">1. Import Contacts</h3>
              <p style="color:var(--text-s); font-size: 0.85rem; margin-bottom: 16px;">Upload your Customers and Vendors CSV.</p>

              <div style="display:flex; gap: 12px; align-items: center;">
                <input type="file" id="import-contacts-csv" accept=".csv" class="form-control" style="flex: 1;">
                <button class="btn btn-primary" onclick="SettingsModule.uploadCSV('contacts')">Upload & Import</button>
              </div>
            </div>
          </div>

          <!-- AI Assistant Panel (hidden by default, shown when AI tab selected) -->
          <div id="settings-ai-panel" style="display:none; margin-top: 0;">
            ${SettingsModule._aiPanelHTML()}
          </div>
        </div>
      </div>
    `;
  },

  _aiPanelHTML() {
    return `
      <div class="card">
        <div class="card-header">
          <span class="card-title">AI Assistant — Local Setup</span>
          <span id="ai-status-badge" style="margin-left:12px;font-size:.75rem;padding:2px 10px;border-radius:10px;background:#fef3c7;color:#92400e">Checking…</span>
        </div>

        <!-- Quick-start instructions -->
        <div style="background:var(--bg,#f8fafc);border:1px solid var(--border,#e2e8f0);border-radius:8px;padding:14px;margin-bottom:20px;font-size:.82rem;color:var(--text-s)">
          <strong style="color:var(--text)">Quick start (runs 100% locally, no API key needed):</strong>
          <ol style="margin:8px 0 0 16px;line-height:1.8">
            <li>Install Ollama → <code style="background:rgba(0,0,0,.05);padding:1px 5px;border-radius:3px">https://ollama.com</code></li>
            <li>Pull Gemma: <code style="background:rgba(0,0,0,.05);padding:1px 5px;border-radius:3px">ollama pull gemma3:4b</code></li>
            <li>Install Piper TTS → <code style="background:rgba(0,0,0,.05);padding:1px 5px;border-radius:3px">https://github.com/rhasspy/piper/releases</code></li>
            <li>Start AI service: <code style="background:rgba(0,0,0,.05);padding:1px 5px;border-radius:3px">cd ai-service && npm install && npm start</code></li>
          </ol>
        </div>

        <!-- Ollama / Model -->
        <div class="form-section-title">Ollama LLM (local, no API key)</div>
        <div class="form-row">
          <div class="form-group">
            <label class="form-label">Ollama URL</label>
            <input type="text" class="form-control" id="ai-ollama-url" placeholder="http://localhost:11434" value="http://localhost:11434">
          </div>
          <div class="form-group">
            <label class="form-label">Model</label>
            <div style="display:flex;gap:8px">
              <input type="text" class="form-control" id="ai-llm-model"
                placeholder="gemma3:4b" value="gemma3:4b" style="flex:1"
                title="Examples: gemma3:4b · gemma3:4b-it-q4_K_M · gemma3:2b-it-q4_0 · gemma4:2b">
              <button class="btn btn-secondary" onclick="SettingsModule.testAIConnection()">Test</button>
            </div>
            <div style="font-size:.72rem;color:var(--text-s);margin-top:3px">
              Quantized variants (less RAM): <code>gemma3:4b-it-q4_K_M</code> &nbsp;·&nbsp; <code>gemma3:2b-it-q4_0</code>
            </div>
          </div>
        </div>

        <!-- Piper TTS -->
        <div class="form-section-title" style="margin-top:20px">
          Piper TTS
          <span style="font-weight:400;font-size:.75rem;color:var(--text-s);margin-left:8px">local, offline, no API key</span>
        </div>
        <div class="form-row">
          <div class="form-group">
            <label class="form-label">Piper binary path</label>
            <input type="text" class="form-control" id="ai-piper-bin" placeholder="piper" value="piper">
            <div style="font-size:.72rem;color:var(--text-s);margin-top:3px">Full path if not in PATH, e.g. <code>/usr/local/bin/piper</code></div>
          </div>
          <div class="form-group">
            <label class="form-label">Voice model (.onnx)</label>
            <input type="text" class="form-control" id="ai-piper-model"
              placeholder="/models/en_IN-indic_voices-medium.onnx">
            <div style="font-size:.72rem;color:var(--text-s);margin-top:3px">
              Download voices: <code>https://github.com/rhasspy/piper/releases</code>
              &nbsp;·&nbsp; Recommended: <code>en_IN-indic_voices-medium.onnx</code>
            </div>
          </div>
        </div>
        <div class="form-row">
          <div class="form-group">
            <label class="form-label">TTS Provider</label>
            <select class="form-control" id="ai-tts">
              <option value="piper">Piper (local, recommended)</option>
              <option value="browser">Browser SpeechSynthesis (fallback)</option>
            </select>
          </div>
          <div class="form-group">
            <label class="form-label">STT Provider</label>
            <select class="form-control" id="ai-stt">
              <option value="browser">Browser Web Speech (free, default)</option>
              <option value="whisper">OpenAI Whisper API</option>
            </select>
          </div>
        </div>
        <div class="form-group">
          <label class="form-label">OpenAI API Key <span style="font-weight:400;color:var(--text-s)">(only needed if STT = Whisper)</span></label>
          <input type="password" class="form-control" id="ai-openai-key" placeholder="sk-… (optional)">
        </div>

        <!-- Z3 -->
        <div class="form-section-title" style="margin-top:20px">Z3 Financial Verification</div>
        <div style="display:flex;align-items:center;gap:12px;margin-bottom:8px">
          <label style="display:flex;align-items:center;gap:8px;cursor:pointer;font-size:.875rem">
            <input type="checkbox" id="ai-z3-enabled" checked style="width:16px;height:16px">
            Enable Z3 constraint verification on all financial responses
          </label>
        </div>
        <div style="font-size:.75rem;color:var(--text-s)">
          When enabled, every invoice/payment figure is verified with Z3 before display.
          Verified numbers appear <mark style="background:#d1fae5;color:#065f46;padding:0 3px;border-radius:2px;font-weight:600;border-bottom:2px solid #34d399">highlighted in green</mark>.
          Requires <code>npm install</code> in ai-service (z3-solver is included).
        </div>

        <!-- WhatsApp -->
        <div class="form-section-title" style="margin-top:20px">WhatsApp Business Agent</div>
        <p style="color:var(--text-s);font-size:.8rem;margin-bottom:12px">
          Enables the AI to send payment reminders and alerts via WhatsApp. Always requires your confirmation before sending.
          You are responsible for Meta API setup and billing.
        </p>
        <div class="form-row">
          <div class="form-group">
            <label class="form-label">WhatsApp Bearer Token</label>
            <input type="password" class="form-control" id="ai-wa-key" placeholder="EAAxxxxxxxx…">
          </div>
          <div class="form-group">
            <label class="form-label">Phone Number ID</label>
            <input type="text" class="form-control" id="ai-wa-phone-id" placeholder="1234567890123456">
          </div>
        </div>

        <!-- AI Service URL -->
        <div class="form-section-title" style="margin-top:20px">AI Service</div>
        <div class="form-group">
          <label class="form-label">AI Service URL</label>
          <input type="text" class="form-control" id="ai-service-url" value="http://localhost:9742" placeholder="http://localhost:9742">
        </div>

        <div style="margin-top:24px;padding-top:20px;border-top:1px solid var(--border-l);display:flex;gap:10px;justify-content:flex-end">
          <button class="btn btn-secondary" onclick="SettingsModule.loadAIConfig()">Reset</button>
          <button class="btn btn-primary" onclick="SettingsModule.saveAIConfig()">Save AI Settings</button>
        </div>
      </div>
    `;
  },

  afterRender() {
    // Wire up sidebar tab links
    document.querySelectorAll('.settings-tab-link').forEach(link => {
      link.addEventListener('click', (e) => {
        e.preventDefault();
        const tab = link.dataset.tab;
        this._switchTab(tab);
      });
    });
    // Check AI service status
    this._checkAIStatus();
  },

  _switchTab(tab) {
    this._activeTab = tab;
    // Update sidebar highlight
    document.querySelectorAll('.settings-tab-link').forEach(l => {
      const active = l.dataset.tab === tab;
      l.style.background = active ? 'var(--primary-l)' : '';
      l.style.color = active ? 'var(--primary)' : 'var(--text)';
      l.style.fontWeight = active ? '600' : '';
      l.style.borderLeft = active ? '3px solid var(--primary)' : '';
    });

    // Show/hide main panels
    const orgCard = document.querySelector('#settings-ai-panel')?.parentElement?.querySelector('.card:first-child');
    const migCard = document.querySelector('#settings-ai-panel')?.previousElementSibling;
    const aiPanel = document.getElementById('settings-ai-panel');

    if (tab === 'ai') {
      if (orgCard) orgCard.style.display = 'none';
      if (migCard) migCard.style.display = 'none';
      if (aiPanel) { aiPanel.style.display = ''; this.loadAIConfig(); }
    } else {
      if (orgCard) orgCard.style.display = '';
      if (migCard) migCard.style.display = tab === 'migration' ? '' : 'none';
      if (aiPanel) aiPanel.style.display = 'none';
    }
  },

  async _checkAIStatus() {
    const badge = document.getElementById('ai-status-badge');
    if (!badge) return;
    const url = localStorage.getItem('qmanage_ai_url') || 'http://localhost:9742';
    try {
      const res = await fetch(`${url}/health`);
      const data = await res.json();
      if (data.status === 'ok') {
        badge.textContent = `Running ✓  ${data.model}`;
        badge.style.background = '#d1fae5';
        badge.style.color      = '#065f46';
      }
    } catch {
      badge.textContent       = 'Not running — run: npm start in ai-service/';
      badge.style.background  = '#fee2e2';
      badge.style.color       = '#991b1b';
    }
  },

  async loadAIConfig() {
    const svcUrl = localStorage.getItem('qmanage_ai_url') || 'http://localhost:9742';
    const set    = (id, val) => { const el = document.getElementById(id); if (el && val != null) el.value = val; };
    set('ai-service-url', svcUrl);

    try {
      const res = await fetch(`${svcUrl}/config`);
      if (!res.ok) return;
      const cfg = await res.json();

      set('ai-ollama-url', cfg.ollama_url);
      set('ai-llm-model',  cfg.llm_model);
      set('ai-tts',        cfg.tts_provider);
      set('ai-stt',        cfg.stt_provider);
      set('ai-piper-bin',  cfg.piper_binary);
      set('ai-piper-model',cfg.piper_model);
      if (cfg.whatsapp_phone_id) set('ai-wa-phone-id', cfg.whatsapp_phone_id);

      const z3cb = document.getElementById('ai-z3-enabled');
      if (z3cb) z3cb.checked = cfg.z3_enabled !== false;
    } catch { /* service not running yet */ }
  },

  async saveAIConfig() {
    const serviceUrl = (document.getElementById('ai-service-url')?.value || 'http://localhost:9742').trim();
    localStorage.setItem('qmanage_ai_url', serviceUrl);

    // Also cache tts_provider for the chatbot to read without hitting the service
    const ttsProv = document.getElementById('ai-tts')?.value || 'piper';
    try { localStorage.setItem('qmanage_ai_cfg', JSON.stringify({ tts_provider: ttsProv })); } catch {}

    const body = {
      ollama_url:   document.getElementById('ai-ollama-url')?.value?.trim() || 'http://localhost:11434',
      llm_model:    document.getElementById('ai-llm-model')?.value?.trim()  || 'gemma3:4b',
      tts_provider: ttsProv,
      stt_provider: document.getElementById('ai-stt')?.value || 'browser',
      piper_binary: document.getElementById('ai-piper-bin')?.value?.trim()   || 'piper',
      piper_model:  document.getElementById('ai-piper-model')?.value?.trim() || '',
      z3_enabled:   document.getElementById('ai-z3-enabled')?.checked !== false,
      backend_url:  localStorage.getItem('qmanage_server') || 'http://localhost:9741/api'
    };
    const oaiKey = document.getElementById('ai-openai-key')?.value?.trim();
    if (oaiKey) body.openai_api_key = oaiKey;
    const waKey = document.getElementById('ai-wa-key')?.value?.trim();
    if (waKey) body.whatsapp_api_key = waKey;
    const waPhone = document.getElementById('ai-wa-phone-id')?.value?.trim();
    if (waPhone) body.whatsapp_phone_id = waPhone;

    try {
      const res = await fetch(`${serviceUrl}/config`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body)
      });
      if (res.ok) {
        QManage.toast('AI settings saved', 'success');
        this._checkAIStatus();
      } else {
        QManage.toast('Save failed — is AI service running?', 'error');
      }
    } catch {
      QManage.toast('AI service not reachable at ' + serviceUrl, 'error');
    }
  },

  async testAIConnection() {
    const serviceUrl = (document.getElementById('ai-service-url')?.value || 'http://localhost:9742').trim();

    // Push current model setting before testing
    const model = document.getElementById('ai-llm-model')?.value?.trim() || 'gemma3:4b';
    const ollamaUrl = document.getElementById('ai-ollama-url')?.value?.trim() || 'http://localhost:11434';
    await fetch(`${serviceUrl}/config`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ llm_model: model, ollama_url: ollamaUrl })
    }).catch(() => {});

    try {
      QManage.toast(`Testing ${model} via Ollama…`, 'info');
      const res = await fetch(`${serviceUrl}/chat`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          message:    'Reply with exactly the word: OK',
          session_id: 'settings_test',
          auth_token: Auth.getToken()
        })
      });
      const data = await res.json();
      if (data.error) {
        QManage.toast('Error: ' + data.error, 'error');
      } else if (data.reply) {
        QManage.toast(`Gemma responded: "${data.reply.slice(0, 60)}"`, 'success');
      }
    } catch (err) {
      QManage.toast('Test failed: ' + err.message, 'error');
    }
  },

  async uploadCSV(type) {
    const fileInput = document.getElementById('import-' + type + '-csv');
    if (!fileInput.files.length) {
      QManage.toast('Please select a CSV file first', 'error');
      return;
    }

    const file = fileInput.files[0];
    const text = await file.text();

    try {
      QManage.toast('Uploading to C Backend...', 'info');
      const res = await API.post('/migration/upload', text, {
        'X-Import-Type': type,
        'Content-Type': 'text/csv'
      });
      
      QManage.toast(`Success! Imported ${res.rows_imported} ${type}.`, 'success');
    } catch (e) {
      QManage.toast('Import failed (is backend running?): ' + e.message, 'error');
    }
  }
};
