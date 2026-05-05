/* Q Manage AI — Tool definitions, executor, and Z3 financial verification
 *
 * All DB queries hit the real C backend. No data is ever fabricated.
 * Z3 formally verifies the consistency of returned financial figures.
 */

// ── Tool definitions (Anthropic-style; server.js converts to OpenAI format) ──

const tools = [
  {
    name: 'query_invoices',
    description: 'Fetch invoice records from the database. Use for questions about sales, billing, outstanding amounts, or specific customer invoices.',
    input_schema: {
      type: 'object',
      properties: {
        status: {
          type: 'string',
          enum: ['draft', 'sent', 'paid', 'overdue', 'void'],
          description: 'Filter by invoice status'
        },
        contact_name: {
          type: 'string',
          description: 'Filter by customer name (partial match)'
        },
        limit: { type: 'number', description: 'Max results (default 20)' }
      }
    }
  },
  {
    name: 'query_payments',
    description: 'Fetch payment records from the database. Use for questions about payments received, payment history, or who paid and when.',
    input_schema: {
      type: 'object',
      properties: {
        contact_name: { type: 'string', description: 'Filter by customer name' },
        limit: { type: 'number', description: 'Max results (default 20)' }
      }
    }
  },
  {
    name: 'query_contacts',
    description: 'Fetch contact records (customers or vendors) from the database.',
    input_schema: {
      type: 'object',
      properties: {
        name: { type: 'string', description: 'Search by name' },
        type: { type: 'string', enum: ['customer', 'vendor'], description: 'Contact type filter' },
        limit: { type: 'number', description: 'Max results (default 20)' }
      }
    }
  },
  {
    name: 'query_bills',
    description: 'Fetch vendor bill records. Use for questions about payables, vendor invoices, or bills due.',
    input_schema: {
      type: 'object',
      properties: {
        status: { type: 'string', enum: ['draft', 'open', 'paid', 'overdue', 'void'] },
        limit: { type: 'number', description: 'Max results (default 20)' }
      }
    }
  },
  {
    name: 'query_expenses',
    description: 'Fetch expense records from the database.',
    input_schema: {
      type: 'object',
      properties: {
        limit: { type: 'number', description: 'Max results (default 20)' }
      }
    }
  },
  {
    name: 'get_dashboard',
    description: 'Get a high-level business summary — total revenue, outstanding receivables, payables, recent activity.',
    input_schema: { type: 'object', properties: {} }
  },
  {
    name: 'create_reminder',
    description: 'Create a reminder for a payment, invoice follow-up, or any custom task.',
    input_schema: {
      type: 'object',
      required: ['title', 'due_at'],
      properties: {
        title:        { type: 'string', description: 'Reminder text, e.g. "Remind Ram: rent due"' },
        due_at:       { type: 'string', description: 'ISO 8601 datetime when reminder fires' },
        contact_name: { type: 'string', description: 'Contact this reminder is about' },
        channel:      { type: 'string', enum: ['in_app', 'whatsapp'], description: 'Default: in_app' }
      }
    }
  },
  {
    name: 'send_whatsapp',
    description: [
      'Prepare a WhatsApp message to a contact.',
      'ALWAYS requires user confirmation — never sends automatically.',
      'Auto-looks up phone number from contacts if not provided.',
      'Compose a clear, professional message based on the context (payment due, reminder, etc.).'
    ].join(' '),
    input_schema: {
      type: 'object',
      required: ['contact_name', 'message'],
      properties: {
        contact_name: { type: 'string', description: 'Recipient name' },
        phone:        { type: 'string', description: 'E.164 phone (e.g. +919876543210). Looked up automatically if omitted.' },
        message:      { type: 'string', description: 'Message body — make it clear and professional' }
      }
    }
  }
];

// ── Tool executor — always hits the real C backend ────────────────────────────

async function executeTool(name, input, authToken, backendUrl) {
  const headers = {
    'Content-Type': 'application/json',
    'X-Auth-Token': authToken
  };

  try {
    switch (name) {

      case 'query_invoices': {
        const p = new URLSearchParams({ limit: input.limit || 20 });
        if (input.status) p.set('status', input.status);
        const r = await fetch(`${backendUrl}/invoices?${p}`, { headers });
        if (!r.ok) return { error: `Backend error ${r.status}` };
        const raw = await r.json();
        const all = Array.isArray(raw) ? raw : (raw.data || []);
        const rows = input.contact_name
          ? all.filter(i => i.contact_name?.toLowerCase().includes(input.contact_name.toLowerCase()))
          : all;
        return { count: rows.length, invoices: rows };
      }

      case 'query_payments': {
        const p = new URLSearchParams({ limit: input.limit || 20 });
        const r = await fetch(`${backendUrl}/payments?${p}`, { headers });
        if (!r.ok) return { error: `Backend error ${r.status}` };
        const raw = await r.json();
        const all = Array.isArray(raw) ? raw : (raw.data || []);
        const rows = input.contact_name
          ? all.filter(p2 => p2.contact_name?.toLowerCase().includes(input.contact_name.toLowerCase()))
          : all;
        return { count: rows.length, payments: rows };
      }

      case 'query_contacts': {
        const p = new URLSearchParams({ limit: input.limit || 20 });
        if (input.type) p.set('type', input.type);
        const r = await fetch(`${backendUrl}/contacts?${p}`, { headers });
        if (!r.ok) return { error: `Backend error ${r.status}` };
        const raw = await r.json();
        const all = Array.isArray(raw) ? raw : (raw.data || []);
        const rows = input.name
          ? all.filter(c => c.name?.toLowerCase().includes(input.name.toLowerCase()))
          : all;
        return { count: rows.length, contacts: rows };
      }

      case 'query_bills': {
        const p = new URLSearchParams({ limit: input.limit || 20 });
        if (input.status) p.set('status', input.status);
        const r = await fetch(`${backendUrl}/bills?${p}`, { headers });
        if (!r.ok) return { error: `Backend error ${r.status}` };
        const raw = await r.json();
        const rows = Array.isArray(raw) ? raw : (raw.data || []);
        return { count: rows.length, bills: rows };
      }

      case 'query_expenses': {
        const p = new URLSearchParams({ limit: input.limit || 20 });
        const r = await fetch(`${backendUrl}/expenses?${p}`, { headers });
        if (!r.ok) return { error: `Backend error ${r.status}` };
        const raw = await r.json();
        const rows = Array.isArray(raw) ? raw : (raw.data || []);
        return { count: rows.length, expenses: rows };
      }

      case 'get_dashboard': {
        const r = await fetch(`${backendUrl}/dashboard`, { headers });
        if (!r.ok) return { error: `Backend error ${r.status}` };
        return r.json();
      }

      case 'create_reminder': {
        const r = await fetch(`${backendUrl}/reminders`, {
          method: 'POST', headers,
          body: JSON.stringify({
            title:        input.title,
            due_at:       Math.floor(new Date(input.due_at).getTime() / 1000),
            channel:      input.channel || 'in_app',
            contact_name: input.contact_name || ''
          })
        });
        if (!r.ok) return { error: 'Failed to create reminder' };
        return { ok: true, message: `Reminder set: "${input.title}"` };
      }

      case 'send_whatsapp': {
        // Auto-lookup phone from contacts when not provided
        let phone = input.phone || null;
        if (!phone && input.contact_name) {
          const r = await fetch(`${backendUrl}/contacts?limit=100`, { headers });
          if (r.ok) {
            const raw = await r.json();
            const contacts = Array.isArray(raw) ? raw : (raw.data || []);
            const match = contacts.find(c =>
              c.name?.toLowerCase().includes(input.contact_name.toLowerCase())
            );
            phone = match?.phone || match?.mobile || null;
          }
        }
        // Always return pending — never auto-send
        return {
          status:       'needs_confirmation',
          contact_name: input.contact_name,
          phone:        phone || 'not found',
          message:      input.message,
          phone_found:  !!phone
        };
      }

      default:
        return { error: `Unknown tool: ${name}` };
    }
  } catch (err) {
    return { error: `Tool failed: ${err.message}` };
  }
}

// ── Z3 financial verification ─────────────────────────────────────────────────
// Lazy-initialise Z3 once; reuse the WASM instance across calls.

let _z3Cache = null;
async function _z3() {
  if (_z3Cache) return _z3Cache;
  const { init } = require('z3-solver');
  _z3Cache = await init();
  return _z3Cache;
}

async function verifyWithZ3(toolResults) {
  const result = {
    z3_available: false,
    checks_run:    0,
    checks_passed: 0,
    failures:      []
  };

  try {
    const Z3 = await _z3();
    result.z3_available = true;

    for (const { tool, result: data } of toolResults) {

      // ── Invoice checks: balance_due == total_amount - amount_paid ──────────
      if (tool === 'query_invoices' && data.invoices) {
        for (const inv of data.invoices.slice(0, 15)) {
          if (inv.total_amount == null || inv.balance_due == null) continue;
          result.checks_run++;

          const { Solver, Int } = Z3.Context(`inv_${inv.id || result.checks_run}`);
          const solver = new Solver();
          const T = Int.val(inv.total_amount || 0);
          const P = Int.val(inv.amount_paid  || 0);
          const D = Int.val(inv.balance_due  || 0);

          // Formal constraint: balance_due = total - paid
          solver.add(D.eq(T.sub(P)));
          const sat = await solver.check();

          if (sat === 'sat') {
            result.checks_passed++;
          } else {
            result.failures.push(
              `Invoice ${inv.invoice_num || inv.id}: balance_due (${inv.balance_due}) ≠ total (${inv.total_amount}) − paid (${inv.amount_paid})`
            );
          }
        }
      }

      // ── Bill checks: same pattern ─────────────────────────────────────────
      if (tool === 'query_bills' && data.bills) {
        for (const bill of data.bills.slice(0, 15)) {
          if (bill.total_amount == null || bill.balance_due == null) continue;
          result.checks_run++;

          const { Solver, Int } = Z3.Context(`bill_${bill.id || result.checks_run}`);
          const solver = new Solver();
          solver.add(
            Int.val(bill.balance_due || 0)
              .eq(Int.val(bill.total_amount || 0).sub(Int.val(bill.amount_paid || 0)))
          );
          const sat = await solver.check();
          if (sat === 'sat') result.checks_passed++;
          else result.failures.push(`Bill ${bill.bill_num || bill.id}: balance inconsistency`);
        }
      }

      // ── Payment positivity checks ─────────────────────────────────────────
      if (tool === 'query_payments' && data.payments) {
        for (const pay of data.payments.slice(0, 15)) {
          if (!pay.amount) continue;
          result.checks_run++;
          const { Solver, Int } = Z3.Context(`pay_${pay.id || result.checks_run}`);
          const solver = new Solver();
          // Payment amount must be positive
          solver.add(Int.val(pay.amount).gt(Int.val(0)));
          const sat = await solver.check();
          if (sat === 'sat') result.checks_passed++;
          else result.failures.push(`Payment ${pay.id}: non-positive amount (${pay.amount})`);
        }
      }

      // ── Dashboard aggregate cross-check ───────────────────────────────────
      if (tool === 'get_dashboard' && data.total_receivable != null && data.total_revenue != null) {
        result.checks_run++;
        // total_receivable <= total_revenue (can't owe more than ever invoiced)
        const { Solver, Int } = Z3.Context('dash');
        const solver = new Solver();
        solver.add(Int.val(data.total_receivable).le(Int.val(data.total_revenue)));
        const sat = await solver.check();
        if (sat === 'sat') result.checks_passed++;
        else result.failures.push('Dashboard: receivable exceeds total revenue');
      }
    }
  } catch (err) {
    // z3-solver not installed or WASM error — degrade gracefully
    result.z3_available = false;
    result.z3_error = err.message;
  }

  result.verified = result.checks_run > 0 && result.checks_passed === result.checks_run;
  return result;
}

// ── Extract all numeric values from tool results (source-of-truth set) ────────
// Used by the frontend to highlight numbers in the LLM response.

function extractSourceNumbers(toolResults) {
  const nums = new Set();

  function walk(val) {
    if (val == null) return;
    if (typeof val === 'number' && isFinite(val) && val > 0) {
      nums.add(val);
      // Add rupee equivalent when value looks like paisa (divisible by 100)
      if (Number.isInteger(val) && val >= 100) {
        nums.add(val / 100);
        nums.add(Math.round(val / 100));
      }
    } else if (Array.isArray(val)) {
      val.forEach(walk);
    } else if (typeof val === 'object') {
      Object.values(val).forEach(walk);
    }
  }

  for (const { result } of toolResults) walk(result);
  return Array.from(nums);
}

module.exports = { tools, executeTool, verifyWithZ3, extractSourceNumbers };
