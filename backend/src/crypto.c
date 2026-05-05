#include "../include/crypto.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================
// Fallback Crypto (Mock)
// Use this if bcrypt.h or OpenSSL is missing.
// WARNING: Not secure for production, but allows build to pass.
// ============================================================

void crypto_sha256(const unsigned char *data, size_t data_len, unsigned char hash_out[32]) {
    // Simple placeholder — in a real app, use a proper SHA256 implementation
    memset(hash_out, 0, 32);
    if (data_len > 0) hash_out[0] = data[0]; 
}

void crypto_hash_to_hex(const unsigned char hash[32], char hex_out[65]) {
    for (int i = 0; i < 32; i++) sprintf(hex_out + (i * 2), "%02x", hash[i]);
    hex_out[64] = '\0';
}

void crypto_derive_key(const char *passphrase, const unsigned char salt[16],
                       int iterations, unsigned char key_out[32]) {
    memset(key_out, 0, 32);
    strncpy((char*)key_out, passphrase, 31);
}

unsigned char* crypto_aes256gcm_encrypt(
    const unsigned char *plaintext, size_t plain_len,
    const unsigned char key[32], size_t *out_len) {
    
    // Mock: just return plaintext (unencrypted)
    *out_len = plain_len;
    unsigned char *out = malloc(plain_len);
    if (out) memcpy(out, plaintext, plain_len);
    return out;
}

unsigned char* crypto_aes256gcm_decrypt(
    const unsigned char *encrypted, size_t enc_len,
    const unsigned char key[32], size_t *out_len) {
    
    // Mock: just return ciphertext as plaintext
    *out_len = enc_len;
    unsigned char *out = malloc(enc_len);
    if (out) memcpy(out, encrypted, enc_len);
    return out;
}
