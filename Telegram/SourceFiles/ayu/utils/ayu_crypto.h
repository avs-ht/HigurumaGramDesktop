// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Privacy strip: at-rest field encryption for the local message archive
// (tdata/ayudata.db). See ayu_crypto.cpp for the threat model this covers
// (and does not cover).
#pragma once

#include <string>
#include <vector>

namespace AyuCrypto {

// AES-256-GCM with a random 12-byte nonce per call. Output layout is
// [12-byte IV][16-byte tag][ciphertext]. The key lives in
// tdata/ayudata.key, generated on first use.
//
// Decrypt* fails open: if the input isn't a well-formed encrypted blob
// (wrong size, bad tag, not valid base64 for the string variant), it is
// returned unchanged. This is deliberate — it lets rows written before
// encryption was added keep reading correctly instead of throwing the
// user's archive away, at the cost of not being able to tell "legacy
// plaintext" apart from "corrupted ciphertext" from the return value alone.
//
// This protects the sqlite file from being casually opened in a generic
// SQLite browser or grepped off disk. It does NOT protect against an
// attacker who also finds tdata/ayudata.key (which sits right next to the
// database) — for protection against a stolen/seized device, this must be
// combined with full-disk encryption (FileVault/BitLocker/LUKS).
[[nodiscard]] std::vector<char> EncryptBlob(const std::vector<char> &plaintext);
[[nodiscard]] std::vector<char> DecryptBlob(const std::vector<char> &stored);

// Same as above, base64-encoded so the result is always valid to bind as
// SQLite TEXT (sqlite_orm binds std::string columns as TEXT).
[[nodiscard]] std::string EncryptString(const std::string &plaintext);
[[nodiscard]] std::string DecryptString(const std::string &stored);

} // namespace AyuCrypto
