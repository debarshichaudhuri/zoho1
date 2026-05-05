/* Q Manage — AI Chatbot + Voice Assistant
 *
 * LLM  : Ollama/Gemma via ai-service on port 9742
 * TTS  : Piper (via service) → browser SpeechSynthesis fallback
 * STT  : Browser Web Speech API → Whisper API fallback
 * Verify: Z3-checked numbers are highlighted green in every response
 */

const ChatbotModule = (() => {
  // ── State ─────────────────────────────────────────────────────────────────
  let isOpen       = false;
  let voiceOutput  = false;
  let isRecording  = false;
  let recognition  = null;
  let sessionId    = localStorage.getItem('qm_chat_session') || _newSession();
  let pendingWA    = null;

  function _newSession() {
    const id = Date.now().toString(36) + Math.random().toString(36).slice(2);
    localStorage.setItem('qm_chat_session', id);
    return id;
  }

  function _aiUrl() {
    return localStorage.getItem('qmanage_ai_url') || 'http://localhost:9742';
  }

  // ── Init ──────────────────────────────────────────────────────────────────
  function init() {
    _injectStyles();
    _buildDOM();
    _bindEvents();
    _addMsg('assistant', 'Hi! Ask me about your invoices, payments, contacts — or how to use any feature.');
  }

  // ── CSS (injected once) ───────────────────────────────────────────────────
  function _injectStyles() {
    if (document.getElementById('qm-chatbot-css')) return;
    const s = document.createElement('style');
    s.id = 'qm-chatbot-css';
    s.textContent = `
      /* ── Floating button ── */
      #chat-fab {
        position:fixed; bottom:24px; right:24px; z-index:9000;
        width:52px; height:52px; border-radius:50%;
        background:var(--primary,#2563eb); color:#fff; border:none;
        cursor:pointer; box-shadow:0 4px 16px rgba(37,99,235,.4);
        display:flex; align-items:center; justify-content:center;
        transition:transform .2s, box-shadow .2s;
      }
      #chat-fab:hover { transform:scale(1.08); box-shadow:0 6px 20px rgba(37,99,235,.5); }
      #chat-fab.open  { background:#64748b; }

      /* ── Panel ── */
      #chat-panel {
        position:fixed; bottom:88px; right:24px; z-index:8999;
        width:370px; max-height:540px;
        background:var(--card,#fff); border-radius:16px;
        box-shadow:0 8px 40px rgba(0,0,0,.18);
        display:flex; flex-direction:column; overflow:hidden;
        transition:opacity .2s,transform .2s;
      }
      #chat-panel.hidden { opacity:0; transform:translateY(12px); pointer-events:none; }

      /* ── Header ── */
      .chat-hdr {
        display:flex; align-items:center; justify-content:space-between;
        padding:12px 16px; background:var(--primary,#2563eb); color:#fff; flex-shrink:0;
      }
      .chat-hdr-title { font-weight:600; font-size:.95rem; display:flex; align-items:center; gap:8px; }
      .chat-hdr-dot   { width:8px; height:8px; border-radius:50%; background:#4ade80; animation:qm-pulse 2s infinite; }
      @keyframes qm-pulse { 0%,100%{opacity:1} 50%{opacity:.35} }
      .chat-hdr-btns  { display:flex; gap:4px; }
      .chat-ibtn {
        background:rgba(255,255,255,.15); border:none; color:#fff;
        width:28px; height:28px; border-radius:6px; cursor:pointer;
        display:flex; align-items:center; justify-content:center; font-size:.8rem;
        transition:background .15s;
      }
      .chat-ibtn:hover  { background:rgba(255,255,255,.28); }
      .chat-ibtn.active { background:rgba(255,255,255,.38); }

      /* ── Messages ── */
      #chat-msgs {
        flex:1; overflow-y:auto; padding:14px 12px;
        display:flex; flex-direction:column; gap:10px; scroll-behavior:smooth;
      }
      #chat-msgs::-webkit-scrollbar { width:4px; }
      #chat-msgs::-webkit-scrollbar-thumb { background:var(--border,#e2e8f0); border-radius:2px; }

      .chat-msg          { display:flex; flex-direction:column; max-width:90%; }
      .chat-msg-user     { align-self:flex-end;   align-items:flex-end; }
      .chat-msg-assistant{ align-self:flex-start;  align-items:flex-start; }
      .chat-msg-error    { align-self:flex-start;  align-items:flex-start; }

      .chat-bubble {
        padding:8px 12px; border-radius:12px;
        font-size:.875rem; line-height:1.55; word-break:break-word;
      }
      .chat-msg-user      .chat-bubble { background:var(--primary,#2563eb); color:#fff; border-bottom-right-radius:4px; }
      .chat-msg-assistant .chat-bubble { background:var(--bg,#f8fafc); border:1px solid var(--border,#e2e8f0); color:var(--text,#1e293b); border-bottom-left-radius:4px; }
      .chat-msg-error     .chat-bubble { background:#fee2e2; color:#991b1b; border:1px solid #fca5a5; }

      /* ── Verified number highlight ── */
      .num-verified {
        background:#d1fae5; color:#065f46;
        padding:0 3px; border-radius:3px; font-weight:600;
        border-bottom:2px solid #34d399; cursor:help;
      }

      /* ── Z3 verification badge ── */
      .chat-verify-badge {
        display:inline-flex; align-items:center; gap:4px;
        font-size:.7rem; color:#065f46; background:#d1fae5;
        border:1px solid #6ee7b7; border-radius:8px;
        padding:2px 8px; margin-top:5px; width:fit-content;
      }
      .chat-verify-badge.warn {
        color:#92400e; background:#fef3c7; border-color:#fcd34d;
      }
      .chat-verify-badge.fail {
        color:#991b1b; background:#fee2e2; border-color:#fca5a5;
      }

      /* ── Typing dots ── */
      .chat-typing-dots { display:flex; gap:4px; padding:10px 14px; }
      .chat-typing-dots span {
        width:6px; height:6px; border-radius:50%;
        background:var(--text-s,#94a3b8); animation:qm-dot .9s infinite;
      }
      .chat-typing-dots span:nth-child(2) { animation-delay:.15s; }
      .chat-typing-dots span:nth-child(3) { animation-delay:.3s; }
      @keyframes qm-dot { 0%,60%,100%{transform:translateY(0)} 30%{transform:translateY(-6px)} }

      /* ── WhatsApp confirm bar ── */
      .chat-wa-bar { display:flex; gap:6px; margin-top:6px; flex-wrap:wrap; }
      .chat-wa-preview {
        font-size:.75rem; color:var(--text-s,#64748b);
        background:var(--bg,#f8fafc); border:1px solid var(--border,#e2e8f0);
        border-radius:6px; padding:4px 8px; margin-top:4px;
        font-style:italic; width:100%;
      }
      .chat-wa-btn {
        padding:4px 14px; border-radius:6px; font-size:.78rem;
        border:none; cursor:pointer; font-weight:500;
      }
      .chat-wa-btn.send   { background:#16a34a; color:#fff; }
      .chat-wa-btn.cancel { background:#e2e8f0; color:#374151; }

      /* ── Input row ── */
      .chat-input-row {
        display:flex; align-items:center; gap:6px;
        padding:10px 12px; border-top:1px solid var(--border,#e2e8f0); flex-shrink:0;
      }
      #chat-input {
        flex:1; border:1px solid var(--border,#e2e8f0); border-radius:8px;
        padding:8px 10px; font-size:.875rem; outline:none;
        background:var(--bg,#f8fafc); color:var(--text,#1e293b);
        transition:border-color .15s;
      }
      #chat-input:focus { border-color:var(--primary,#2563eb); }
      .chat-act-btn {
        width:34px; height:34px; border-radius:8px; border:none;
        background:var(--bg,#f1f5f9); color:var(--text-s,#64748b);
        cursor:pointer; display:flex; align-items:center; justify-content:center;
        transition:background .15s,color .15s; flex-shrink:0;
      }
      .chat-act-btn:hover         { background:var(--primary,#2563eb); color:#fff; }
      .chat-act-btn#chat-send     { background:var(--primary,#2563eb); color:#fff; }
      .chat-act-btn#chat-send:hover { background:#1d4ed8; }
      .chat-act-btn.recording     { background:#ef4444 !important; color:#fff !important; animation:qm-pulse 1s infinite; }

      @media (max-width:420px) {
        #chat-panel { width:calc(100vw - 24px); right:12px; }
        #chat-fab   { right:16px; bottom:16px; }
      }
    `;
    document.head.appendChild(s);
  }

  // ── DOM ───────────────────────────────────────────────────────────────────
  function _buildDOM() {
    const root = document.createElement('div');
    root.id = 'chatbot-root';
    root.innerHTML = `
      <button id="chat-fab" aria-label="Open Q Assistant">
        <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
          <path d="M21 15a2 2 0 0 1-2 2H7l-4 4V5a2 2 0 0 1 2-2h14a2 2 0 0 1 2 2z"/>
        </svg>
      </button>
      <div id="chat-panel" class="hidden" role="dialog" aria-label="Q Assistant">
        <div class="chat-hdr">
          <div class="chat-hdr-title">
            <div class="chat-hdr-dot"></div>Q Assistant
          </div>
          <div class="chat-hdr-btns">
            <button id="chat-voice-toggle" class="chat-ibtn" title="Toggle voice output">
              <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                <polygon points="11 5 6 9 2 9 2 15 6 15 11 19 11 5"/>
                <path d="M15.54 8.46a5 5 0 0 1 0 7.07"/>
                <path d="M19.07 4.93a10 10 0 0 1 0 14.14"/>
              </svg>
            </button>
            <button id="chat-clear" class="chat-ibtn" title="Clear chat">
              <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                <polyline points="3 6 5 6 21 6"/>
                <path d="M19 6l-1 14H6L5 6"/>
                <path d="M10 11v6"/><path d="M14 11v6"/>
              </svg>
            </button>
            <button id="chat-close" class="chat-ibtn" title="Close">
              <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                <line x1="18" y1="6" x2="6" y2="18"/>
                <line x1="6" y1="6" x2="18" y2="18"/>
              </svg>
            </button>
          </div>
        </div>
        <div id="chat-msgs" role="log" aria-live="polite"></div>
        <div class="chat-input-row">
          <input type="text" id="chat-input" placeholder="Ask anything…" autocomplete="off" aria-label="Message">
          <button id="chat-mic" class="chat-act-btn" title="Voice input">
            <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
              <rect x="9" y="2" width="6" height="12" rx="3"/>
              <path d="M5 10a7 7 0 0 0 14 0"/>
              <line x1="12" y1="19" x2="12" y2="22"/>
              <line x1="8"  y1="22" x2="16" y2="22"/>
            </svg>
          </button>
          <button id="chat-send" class="chat-act-btn" title="Send">
            <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
              <line x1="22" y1="2" x2="11" y2="13"/>
              <polygon points="22 2 15 22 11 13 2 9 22 2"/>
            </svg>
          </button>
        </div>
      </div>
    `;
    document.body.appendChild(root);
  }

  function _bindEvents() {
    document.getElementById('chat-fab').onclick          = toggle;
    document.getElementById('chat-close').onclick        = close;
    document.getElementById('chat-clear').onclick        = clearSession;
    document.getElementById('chat-send').onclick         = () => sendMessage();
    document.getElementById('chat-voice-toggle').onclick = _toggleVoice;
    document.getElementById('chat-mic').onclick          = _toggleMic;
    document.getElementById('chat-input').onkeydown = (e) => {
      if (e.key === 'Enter' && !e.shiftKey) { e.preventDefault(); sendMessage(); }
    };
  }

  // ── Open / close ──────────────────────────────────────────────────────────
  function toggle() { isOpen ? close() : open(); }

  function open() {
    isOpen = true;
    document.getElementById('chat-panel').classList.remove('hidden');
    document.getElementById('chat-fab').classList.add('open');
    document.getElementById('chat-input').focus();
  }

  function close() {
    isOpen = false;
    document.getElementById('chat-panel').classList.add('hidden');
    document.getElementById('chat-fab').classList.remove('open');
  }

  async function clearSession() {
    sessionId = _newSession();
    try { await fetch(`${_aiUrl()}/session/${sessionId}`, { method: 'DELETE' }); } catch {}
    document.getElementById('chat-msgs').innerHTML = '';
    _addMsg('assistant', 'Conversation cleared. How can I help?');
  }

  // ── Send message ──────────────────────────────────────────────────────────
  async function sendMessage(override) {
    const input = document.getElementById('chat-input');
    const text  = (override || input.value).trim();
    if (!text) return;
    input.value = '';
    pendingWA = null;

    _addMsg('user', text);
    _setTyping(true);

    try {
      const res = await fetch(`${_aiUrl()}/chat`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          message:    text,
          session_id: sessionId,
          auth_token: Auth.getToken()
        })
      });

      _setTyping(false);

      if (!res.ok) {
        const err = await res.json().catch(() => ({}));
        _addMsg('error', err.error || `Service error (${res.status})`);
        return;
      }

      const data = await res.json();
      _addMsg('assistant', data.reply, data.pending_action, data.verification, data.source_numbers || []);

      if (voiceOutput && data.reply) _speak(data.reply);

    } catch (err) {
      _setTyping(false);
      const hint = (err.message.includes('fetch') || err.message.includes('Failed'))
        ? 'AI service offline — run: cd ai-service && npm start'
        : err.message;
      _addMsg('error', hint);
    }
  }

  // ── Render message ────────────────────────────────────────────────────────
  function _addMsg(role, text, pendingAction, verification, sourceNumbers) {
    const c   = document.getElementById('chat-msgs');
    const el  = document.createElement('div');
    el.className = `chat-msg chat-msg-${role}`;

    const bubble = document.createElement('div');
    bubble.className = 'chat-bubble';

    // Highlight verified numbers in the bubble text
    bubble.innerHTML = sourceNumbers && sourceNumbers.length
      ? _formatWithHighlights(text, sourceNumbers)
      : _fmt(text);

    el.appendChild(bubble);

    // Z3 verification badge
    if (verification) {
      el.appendChild(_verifyBadge(verification));
    }

    // WhatsApp confirmation bar
    if (pendingAction?.type === 'whatsapp') {
      pendingWA = pendingAction;
      const bar = document.createElement('div');
      bar.className = 'chat-wa-bar';

      const preview = document.createElement('div');
      preview.className = 'chat-wa-preview';
      preview.textContent = `"${pendingAction.message}"`;

      const sendBtn = document.createElement('button');
      sendBtn.className = 'chat-wa-btn send';
      sendBtn.textContent = `Send to ${_esc(pendingAction.contact_name)}`;
      sendBtn.onclick = confirmWhatsApp;

      const cancelBtn = document.createElement('button');
      cancelBtn.className = 'chat-wa-btn cancel';
      cancelBtn.textContent = 'Cancel';
      cancelBtn.onclick = () => { bar.remove(); pendingWA = null; };

      bar.appendChild(preview);
      bar.appendChild(sendBtn);
      bar.appendChild(cancelBtn);
      el.appendChild(bar);
    }

    c.appendChild(el);
    c.scrollTop = c.scrollHeight;
  }

  // Build the Z3 verification badge element
  function _verifyBadge(v) {
    const badge = document.createElement('div');

    if (!v.z3_available) {
      // Z3 not installed — silent, no badge
      return document.createDocumentFragment();
    }

    if (v.checks_run === 0) {
      return document.createDocumentFragment();
    }

    if (v.verified) {
      badge.className = 'chat-verify-badge';
      badge.innerHTML = `
        <svg width="10" height="10" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="3">
          <polyline points="20 6 9 17 4 12"/>
        </svg>
        Z3 verified · ${v.checks_passed}/${v.checks_run} checks passed
      `;
    } else {
      badge.className = 'chat-verify-badge fail';
      const issues = v.failures.slice(0, 2).join('; ');
      badge.innerHTML = `
        <svg width="10" height="10" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="3">
          <circle cx="12" cy="12" r="10"/><line x1="12" y1="8" x2="12" y2="12"/><line x1="12" y1="16" x2="12.01" y2="16"/>
        </svg>
        Inconsistency detected · ${issues}
      `;
    }
    return badge;
  }

  // Typing indicator
  function _setTyping(show) {
    const existing = document.getElementById('chat-typing');
    if (show) {
      if (existing) return;
      const el = document.createElement('div');
      el.id = 'chat-typing';
      el.className = 'chat-msg chat-msg-assistant';
      el.innerHTML = '<div class="chat-bubble chat-typing-dots"><span></span><span></span><span></span></div>';
      const c = document.getElementById('chat-msgs');
      c.appendChild(el);
      c.scrollTop = c.scrollHeight;
    } else {
      if (existing) existing.remove();
    }
  }

  // ── Text formatting ───────────────────────────────────────────────────────

  // Highlight numbers that appear in the source (DB) data
  function _formatWithHighlights(text, sourceNumbers) {
    // Build a lookup set — store both paisa and rupee representations
    const src = new Set();
    for (const n of sourceNumbers) {
      src.add(Math.round(n));
      src.add(n.toString());
      src.add(Math.round(n).toString());
    }

    // First apply standard formatting (escaping, markdown)
    const base = _fmt(text);

    // Then wrap numbers that match source data with highlight spans
    return base.replace(
      /([₹]?)([\d,]+(?:\.\d{1,2})?)/g,
      (match, prefix, numStr) => {
        const clean  = numStr.replace(/,/g, '');
        const parsed = parseFloat(clean);
        if (isNaN(parsed)) return match;

        const isVerified = src.has(Math.round(parsed))
          || src.has(Math.round(parsed * 100))   // rupees → paisa
          || src.has(Math.round(parsed / 100));   // paisa  → rupees

        return isVerified
          ? `${prefix}<mark class="num-verified" title="Verified from database">${numStr}</mark>`
          : match;
      }
    );
  }

  // Basic markdown → HTML (no external lib needed)
  function _fmt(text) {
    return (text || '')
      .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
      .replace(/\*\*(.*?)\*\*/g, '<strong>$1</strong>')
      .replace(/\*(.*?)\*/g, '<em>$1</em>')
      .replace(/`([^`]+)`/g, '<code style="background:rgba(0,0,0,.06);padding:1px 4px;border-radius:3px;font-size:.85em">$1</code>')
      .replace(/\n/g, '<br>');
  }

  function _esc(s) {
    return (s || '').replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
  }

  // ── Voice output (TTS) ────────────────────────────────────────────────────
  function _toggleVoice() {
    voiceOutput = !voiceOutput;
    document.getElementById('chat-voice-toggle').classList.toggle('active', voiceOutput);
    if (!voiceOutput && window.speechSynthesis) window.speechSynthesis.cancel();
  }

  function _speak(text) {
    // Strip markdown before speaking
    const plain = text.replace(/\*\*(.*?)\*\*/g, '$1').replace(/`([^`]+)`/g, '$1').replace(/<[^>]+>/g, '');

    // Try Piper via AI service first (better quality)
    const cfg = loadConfig();
    if (cfg?.tts_provider === 'piper') {
      fetch(`${_aiUrl()}/tts`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ text: plain })
      }).then(r => {
        if (!r.ok) throw new Error('tts failed');
        return r.blob();
      }).then(blob => {
        const url   = URL.createObjectURL(blob);
        const audio = new Audio(url);
        audio.play();
        audio.onended = () => URL.revokeObjectURL(url);
      }).catch(() => _browserSpeak(plain));
      return;
    }

    _browserSpeak(plain);
  }

  function _browserSpeak(plain) {
    if (!window.speechSynthesis) return;
    window.speechSynthesis.cancel();
    const utt = new SpeechSynthesisUtterance(plain);
    utt.lang = 'en-IN';
    utt.rate = 1.0;
    const voices   = window.speechSynthesis.getVoices();
    const preferred = voices.find(v => /en.*IN/i.test(v.lang)) || voices.find(v => /en/i.test(v.lang));
    if (preferred) utt.voice = preferred;
    window.speechSynthesis.speak(utt);
  }

  // Lazy-read the AI service config (cached from last settings save)
  function loadConfig() {
    try {
      const s = localStorage.getItem('qmanage_ai_cfg');
      return s ? JSON.parse(s) : {};
    } catch { return {}; }
  }

  // ── Voice input (STT) ─────────────────────────────────────────────────────
  function _toggleMic() { isRecording ? _stopMic() : _startMic(); }

  function _startMic() {
    const SR = window.SpeechRecognition || window.webkitSpeechRecognition;
    if (SR) {
      recognition = new SR();
      recognition.lang = 'en-IN';
      recognition.interimResults = false;
      recognition.onresult = (e) => {
        const t = e.results[0][0].transcript;
        document.getElementById('chat-input').value = t;
        sendMessage(t);
      };
      recognition.onerror = () => _stopMic();
      recognition.onend   = () => _stopMic();
      recognition.start();
      isRecording = true;
      document.getElementById('chat-mic').classList.add('recording');
    } else {
      _startMediaRecorder();
    }
  }

  function _stopMic() {
    if (recognition) { recognition.stop(); recognition = null; }
    isRecording = false;
    const btn = document.getElementById('chat-mic');
    if (btn) btn.classList.remove('recording');
  }

  function _startMediaRecorder() {
    navigator.mediaDevices.getUserMedia({ audio: true }).then(stream => {
      const rec    = new MediaRecorder(stream);
      const chunks = [];
      rec.ondataavailable = e => chunks.push(e.data);
      rec.onstop = async () => {
        stream.getTracks().forEach(t => t.stop());
        const blob = new Blob(chunks, { type: 'audio/webm' });
        const form = new FormData();
        form.append('audio', blob, 'audio.webm');
        try {
          const r = await fetch(`${_aiUrl()}/transcribe`, { method: 'POST', body: form });
          const d = await r.json();
          if (d.text) sendMessage(d.text);
        } catch (err) {
          if (window.QManage) QManage.toast('Transcription failed: ' + err.message, 'error');
        }
        isRecording = false;
        document.getElementById('chat-mic').classList.remove('recording');
      };
      rec.start();
      isRecording = true;
      document.getElementById('chat-mic').classList.add('recording');
      // Auto-stop after 30s; also stop on second tap
      const stopAfter = setTimeout(() => rec.state === 'recording' && rec.stop(), 30000);
      const micBtn = document.getElementById('chat-mic');
      const origClick = micBtn.onclick;
      micBtn.onclick = () => { clearTimeout(stopAfter); rec.stop(); micBtn.onclick = origClick; };
    }).catch(() => {
      if (window.QManage) QManage.toast('Microphone access denied', 'error');
    });
  }

  // ── WhatsApp agent confirmation ───────────────────────────────────────────
  async function confirmWhatsApp() {
    if (!pendingWA) return;
    const { phone, message } = pendingWA;
    pendingWA = null;
    document.querySelectorAll('.chat-wa-bar').forEach(b => b.remove());

    try {
      const r = await fetch(`${_aiUrl()}/whatsapp/send`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ phone, message, auth_token: Auth.getToken() })
      });
      const d = await r.json();
      if (d.ok) {
        _addMsg('assistant', `Message sent to ${phone}.`);
        if (window.QManage) QManage.toast('WhatsApp message sent!', 'success');
      } else {
        _addMsg('error', 'Send failed: ' + (d.error || 'Unknown error'));
      }
    } catch (err) {
      _addMsg('error', 'WhatsApp error: ' + err.message);
    }
  }

  // ── Public API ────────────────────────────────────────────────────────────
  return { init, toggle, open, close, sendMessage, confirmWhatsApp };
})();

// Init after the user authenticates
document.addEventListener('DOMContentLoaded', () => {
  if (typeof Auth !== 'undefined' && Auth.isLoggedIn()) {
    ChatbotModule.init();
  } else {
    document.addEventListener('qmanage:loggedin', () => ChatbotModule.init(), { once: true });
  }
});
