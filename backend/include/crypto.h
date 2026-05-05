#ifndef CRYPTO_H
#define CRYPTO_H

#include "qmanage.h"

// ============================================================
// Cryptography Module — AES-256-GCM + SHA-256
// Uses Windows BCrypt API (zero external dependencies)
// ============================================================

// AES-256-GCM Encryption
// Returns: malloc'd buffer containing [12-byte nonce | 16-byte tag | ciphertext]
// Caller must free() the returned buffer
// *out_len is set to the total output length
unsigned char* crypto_aes256gcm_encrypt(
    const unsigned char *plaintext, size_t plain_len,
    const unsigned char key[32],    // 256-bit key
    size_t *out_len
);

// AES-256-GCM Decryption
// Input: buffer from crypto_aes256gcm_encrypt [nonce | tag | ciphertext]
// Returns: malloc'd plaintext, or NULL if authentication fails
// *out_len is set to the plaintext length
unsigned char* crypto_aes256gcm_decrypt(
    const unsigned char *encrypted, size_t enc_len,
    const unsigned char key[32],
    size_t *out_len
);

// Derive a 256-bit key from a passphrase using PBKDF2-SHA256
// salt should be 16 bytes, iterations recommended >= 100000
void crypto_derive_key(
    const char *passphrase,
    const unsigned char salt[16],
    int iterations,
    unsigned char key_out[32]
);

// SHA-256 hash
// Returns 32-byte hash
void crypto_sha256(
    const unsigned char *data, size_t data_len,
    unsigned char hash_out[32]
);

// Convert 32-byte hash to 64-char hex string
void crypto_hash_to_hex(const unsigned char hash[32], char hex_out[65]);

// ============================================================
// Audit Trail (audit.c functions)
// ============================================================

// Log an auditable action with SHA-256 chain hash
bool audit_log_action(AppState *app, const char *action, const char *entity_type,
                      int64_t entity_id, const char *details);

// Verify audit chain integrity — returns number of broken links (0 = valid)
int audit_verify_chain(AppState *app);

#endif // CRYPTO_H
