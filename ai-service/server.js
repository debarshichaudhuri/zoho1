/*
 * Q Manage AI Service — Port 9742
 *
 * LLM   : Ollama (Gemma local) via OpenAI-compatible API at :11434
 * TTS   : Piper (local binary) → system say/espeak fallback
 * STT   : Browser Web Speech (frontend) → Whisper API fallback
 * Verify: Z3 constraint solver checks every financial figure before the
 *         answer is shown. Verified numbers are highlighted in the UI.
 * WA    : WhatsApp Business API agent (requires user confirmation)
 */

const express  = require('express');
const cors     = require('cors');
const fs       = require('fs');
const path     = require('path');
const os       = require('os');
const { spawn }    = require('child_process');
const { OpenAI }   = require('openai');
const multer       = require('multer');
const { tools, executeTool, verifyWithZ3, extractSourceNumbers } = require('./tools');
const { APP_KNOWLEDGE } = require('./knowledge');

const app    = express();
const upload = multer({ storage: multer.memoryStorage(), limits: { fileSize: 25 * 1024 * 1024 } });

app.use(cors());
app.use(express.json({ limit: '2mb' }));

const CONFIG_FILE = path.join(__dirname, 'config.json');

// ── Session store (in-memory, 2-hour TTL) ─────────────────────────────────────
const sessions = new Map();
setInterval(() => {
  const cutoff = Date.now() - 2 * 60 * 60 * 1000;
  for (const [id, s] of sessions) {
    if (s.lastActive < cutoff) sessions.delete(id);
  }
}, 30 * 60 * 1000);

// ── Config helpers ────────────────────────────────────────────────────────────
function loadConfig() {
  try { return JSON.parse(fs.readFileSync(CONFIG_FILE, 'utf8')); }
  catch { return {}; }
}
function saveConfig(cfg) {
  fs.writeFileSync(CONFIG_FILE, JSON.stringify(cfg, null, 2));
}

// ── Build Ollama client (OpenAI-compatible) ───────────────────────────────────
function buildClient(cfg) {
  const base = (cfg.ollama_url || 'http://localhost:11434').replace(/\/$/, '');
  return new OpenAI({ baseURL: `${base}/v1`, apiKey: 'ollama' });
}

// Convert Anthropic-style tool defs → OpenAI function-calling format
function toOAITools(defs) {
  return defs.map(t => ({
    type: 'function',
    function: {
      name: t.name,
      description: t.description,
      parameters: t.input_schema || { type: 'object', properties: {} }
    }
  }));
}

// ── Routes ────────────────────────────────────────────────────────────────────

app.get('/health', (_req, res) => {
  const cfg = loadConfig();
  res.json({
    status: 'ok',
    service: 'qmanage-ai',
    backend: 'ollama',
    model: cfg.llm_model || 'gemma3:4b',
    tts: cfg.tts_provider || 'piper',
    z3: cfg.z3_enabled !== false
  });
});

app.get('/config', (_req, res) => {
  const cfg = loadConfig();
  res.json({
    ollama_url:        cfg.ollama_url        || 'http://localhost:11434',
    llm_model:         cfg.llm_model         || 'gemma3:4b',
    tts_provider:      cfg.tts_provider      || 'piper',
    piper_binary:      cfg.piper_binary      || 'piper',
    piper_model:       cfg.piper_model       || '',
    stt_provider:      cfg.stt_provider      || 'browser',
    openai_api_key_set: !!(cfg.openai_api_key),
    has_whatsapp:      !!(cfg.whatsapp_api_key && cfg.whatsapp_phone_id),
    whatsapp_phone_id: cfg.whatsapp_phone_id  || '',
    backend_url:       cfg.backend_url        || 'http://localhost:9741/api',
    z3_enabled:        cfg.z3_enabled !== false
  });
});

app.post('/config', (req, res) => {
  const cfg = loadConfig();
  const allowed = [
    'ollama_url', 'llm_model',
    'tts_provider', 'piper_binary', 'piper_model',
    'stt_provider', 'openai_api_key',
    'whatsapp_api_key', 'whatsapp_phone_id',
    'backend_url', 'z3_enabled'
  ];
  for (const key of allowed) {
    if (req.body[key] !== undefined) cfg[key] = req.body[key];
  }
  saveConfig(cfg);
  res.json({ ok: true });
});

// ── Main chat endpoint ────────────────────────────────────────────────────────

app.post('/chat', async (req, res) => {
  const { message, session_id, auth_token } = req.body;
  if (!message || !auth_token) {
    return res.status(400).json({ error: 'message and auth_token required' });
  }

  const cfg = loadConfig();
  const sid = session_id || 'default';

  if (!sessions.has(sid)) sessions.set(sid, { history: [], lastActive: Date.now() });
  const session = sessions.get(sid);
  session.lastActive = Date.now();
  session.history.push({ role: 'user', content: message });

  try {
    const client     = buildClient(cfg);
    const backendUrl = cfg.backend_url || 'http://localhost:9741/api';
    const model      = cfg.llm_model  || 'gemma3:4b';
    const z3Enabled  = cfg.z3_enabled !== false;

    const result = await runAgentLoop(
      client, model,
      session.history.slice(-20),
      auth_token, backendUrl, z3Enabled
    );

    session.history.push({ role: 'assistant', content: result.reply });
    res.json(result);

  } catch (err) {
    console.error('[chat error]', err.message);
    if (err.message.includes('ECONNREFUSED') && (err.message.includes('11434') || err.message.includes('ollama'))) {
      return res.status(503).json({ error: 'Ollama is not running. Start it: ollama serve' });
    }
    if (err.message.includes('model') && err.message.includes('not found')) {
      const m = loadConfig().llm_model || 'gemma3:4b';
      return res.status(503).json({ error: `Model not found. Pull it: ollama pull ${m}` });
    }
    res.status(500).json({ error: err.message });
  }
});

// ── Agent loop ────────────────────────────────────────────────────────────────
// Runs tool calls (real DB queries) until Gemma produces a final text answer.
// Z3 verifies every financial figure before returning.

async function runAgentLoop(client, model, messages, authToken, backendUrl, z3Enabled) {
  const oaiTools = toOAITools(tools);

  let current = [
    { role: 'system', content: APP_KNOWLEDGE },
    ...messages
  ];

  let pendingAction  = null;
  let allToolResults = [];

  for (let round = 0; round < 10; round++) {
    const response = await client.chat.completions.create({
      model,
      messages: current,
      tools: oaiTools,
      tool_choice: 'auto',
      temperature: 0  // deterministic for financial accuracy
    });

    const choice = response.choices[0];
    const msg    = choice.message;

    // No tool calls — model gave a final answer
    if (!msg.tool_calls || msg.tool_calls.length === 0) {
      const reply = msg.content || 'I could not generate a response.';

      // Z3 verification pass
      let verification  = null;
      let sourceNumbers = [];
      if (z3Enabled && allToolResults.length > 0) {
        [verification, sourceNumbers] = await Promise.all([
          verifyWithZ3(allToolResults),
          Promise.resolve(extractSourceNumbers(allToolResults))
        ]);
      }

      return {
        reply,
        session_id: null,
        pending_action: pendingAction || null,
        verification,
        source_numbers: sourceNumbers
      };
    }

    // Execute tool calls
    const toolMsgs = [];
    for (const toolCall of msg.tool_calls) {
      let args = {};
      try { args = JSON.parse(toolCall.function.arguments); } catch {}

      const result = await executeTool(toolCall.function.name, args, authToken, backendUrl);
      allToolResults.push({ tool: toolCall.function.name, result });

      // Capture WhatsApp pending action
      if (toolCall.function.name === 'send_whatsapp' && result.status === 'needs_confirmation') {
        pendingAction = {
          type:         'whatsapp',
          contact_name: result.contact_name,
          phone:        result.phone,
          message:      result.message
        };
      }

      toolMsgs.push({
        role:        'tool',
        tool_call_id: toolCall.id,
        content:     JSON.stringify(result)
      });
    }

    current = [
      ...current,
      { role: 'assistant', content: msg.content || null, tool_calls: msg.tool_calls },
      ...toolMsgs
    ];
  }

  return { reply: 'I was unable to complete this request.', pending_action: pendingAction };
}

// ── TTS: Piper (local) ────────────────────────────────────────────────────────

app.post('/tts', async (req, res) => {
  const { text } = req.body;
  if (!text) return res.status(400).json({ error: 'text required' });

  const cfg      = loadConfig();
  const provider = cfg.tts_provider || 'piper';

  if (provider === 'piper' && cfg.piper_model) {
    try {
      const audio = await piperSynth(text, cfg.piper_binary || 'piper', cfg.piper_model);
      res.set('Content-Type', 'audio/wav');
      return res.send(audio);
    } catch (err) {
      console.error('[piper error]', err.message);
      // fall through to system TTS
    }
  }

  // Fallback: macOS say / Linux espeak-ng
  try {
    const audio = await systemTTS(text);
    res.set('Content-Type', 'audio/wav');
    return res.send(audio);
  } catch (err) {
    res.status(503).json({ error: 'TTS unavailable — install Piper or espeak-ng. ' + err.message });
  }
});

function piperSynth(text, piperBin, modelPath) {
  return new Promise((resolve, reject) => {
    const tmpOut = path.join(os.tmpdir(), `piper_${Date.now()}.wav`);
    const proc   = spawn(piperBin, ['--model', modelPath, '--output_file', tmpOut]);

    let stderr = '';
    proc.stderr.on('data', d => { stderr += d; });
    proc.stdin.write(text);
    proc.stdin.end();

    proc.on('error', (err) => reject(new Error(`Piper binary not found: ${err.message}`)));
    proc.on('close', (code) => {
      if (code !== 0) return reject(new Error(`Piper exited ${code}: ${stderr.slice(0, 200)}`));
      try {
        const buf = fs.readFileSync(tmpOut);
        fs.unlinkSync(tmpOut);
        resolve(buf);
      } catch (e) { reject(e); }
    });
  });
}

function systemTTS(text) {
  return new Promise((resolve, reject) => {
    const tmpOut = path.join(os.tmpdir(), `tts_sys_${Date.now()}.aiff`);
    // macOS: say outputs AIFF; Linux: espeak-ng outputs WAV
    const isLinux = process.platform === 'linux';
    const cmd     = isLinux ? 'espeak-ng' : 'say';
    const args    = isLinux
      ? ['-w', tmpOut, text]
      : ['-o', tmpOut, text];

    const proc = spawn(cmd, args);
    proc.on('error', (e) => reject(new Error(`${cmd} not found: ${e.message}`)));
    proc.on('close', (code) => {
      if (code !== 0) return reject(new Error(`${cmd} failed`));
      try {
        const buf = fs.readFileSync(tmpOut);
        fs.unlinkSync(tmpOut);
        resolve(buf);
      } catch (e) { reject(e); }
    });
  });
}

// ── STT: Whisper fallback (browser handles default) ──────────────────────────

app.post('/transcribe', upload.single('audio'), async (req, res) => {
  if (!req.file) return res.status(400).json({ error: 'No audio file provided' });

  const cfg = loadConfig();
  if (!cfg.openai_api_key) {
    return res.status(503).json({ error: 'OpenAI key not set — needed for Whisper STT' });
  }

  try {
    const { Readable } = require('stream');
    const openai  = new OpenAI({ apiKey: cfg.openai_api_key });
    const stream  = Readable.from(req.file.buffer);
    stream.path   = req.file.mimetype.includes('webm') ? 'audio.webm' : 'audio.wav';

    const tx = await openai.audio.transcriptions.create({
      file: stream, model: 'whisper-1', language: 'en'
    });
    res.json({ text: tx.text });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// ── WhatsApp: confirmed send ──────────────────────────────────────────────────

app.post('/whatsapp/send', async (req, res) => {
  const { phone, message, auth_token } = req.body;
  if (!phone || !message) return res.status(400).json({ error: 'phone and message required' });
  if (!auth_token) return res.status(401).json({ error: 'auth_token required' });

  const cfg = loadConfig();
  if (!cfg.whatsapp_api_key || !cfg.whatsapp_phone_id) {
    return res.status(503).json({ error: 'WhatsApp not configured. Go to Settings → AI Assistant.' });
  }

  // Normalise to E.164 (+91...)
  const digits = phone.replace(/\D/g, '');
  const e164   = digits.startsWith('91') ? `+${digits}` : `+91${digits}`;

  try {
    const r = await fetch(
      `https://graph.facebook.com/v18.0/${cfg.whatsapp_phone_id}/messages`,
      {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${cfg.whatsapp_api_key}`
        },
        body: JSON.stringify({
          messaging_product: 'whatsapp',
          to: e164,
          type: 'text',
          text: { body: message }
        })
      }
    );

    if (!r.ok) {
      const err = await r.json().catch(() => ({}));
      return res.status(502).json({ error: err.error?.message || 'WhatsApp API error' });
    }
    const data = await r.json();
    res.json({ ok: true, message_id: data.messages?.[0]?.id, to: e164 });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// ── Session clear ─────────────────────────────────────────────────────────────

app.delete('/session/:id', (req, res) => {
  sessions.delete(req.params.id);
  res.json({ ok: true });
});

// ── Start ─────────────────────────────────────────────────────────────────────

const PORT = process.env.PORT || 9742;
app.listen(PORT, () => {
  const cfg = loadConfig();
  console.log(`\nQ Manage AI Service  →  http://localhost:${PORT}`);
  console.log(`  Model : ${cfg.llm_model || 'gemma3:4b'}  via  ${cfg.ollama_url || 'http://localhost:11434'}`);
  console.log(`  TTS   : ${cfg.tts_provider || 'piper'}  ${cfg.piper_model ? `(${cfg.piper_model})` : '(no model set)'}`);
  console.log(`  Z3    : ${cfg.z3_enabled !== false ? 'enabled' : 'disabled'}\n`);
});
