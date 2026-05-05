/* Q Manage — Offline Fallback Layer
 * 
 * This module previously held IndexedDB logic for fully offline operation.
 * With the C backend now handling all data via SQLite, this layer is
 * retained as a minimal stub for backward compatibility.
 * 
 * The C backend IS the database. No browser-side DB needed.
 */
const QDB = {
  isReady: true,

  // Stub — all data comes from C API now
  async init() {
    console.log('[QDB] Offline fallback layer ready (all data via C API)');
    return true;
  },

  // Health check passthrough
  async checkBackend() {
    return API.checkHealth();
  }
};
