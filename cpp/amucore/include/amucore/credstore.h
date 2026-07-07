#pragma once

#include <string>

namespace amucore {

// Windows DPAPI credential store, ported from _CredEncrypt/_CredDecrypt in amu.au3.
//
// A credential is encrypted with CryptProtectData over its UTF-8 bytes and stored
// as the string "DPAPI:0x<HEX>" (uppercase hex, matching AutoIt's String(binary)).
// The DPAPI blob is tied to the current Windows user account, so it is useless on
// another account/machine and no key is stored anywhere.
//
// Both functions degrade gracefully (like the AutoIt): on any failure they return a
// value that keeps the app working rather than throwing or losing data.

// Encrypt a plaintext credential. Returns "" for empty input, "DPAPI:0x<HEX>" on
// success, and the untouched plaintext if DPAPI fails.
std::string protect(const std::string& plaintext);

// Decrypt a stored credential. Returns "" for empty input; a legacy plaintext value
// (no "DPAPI:" prefix) is returned unchanged; a "DPAPI:0x<HEX>" value is decrypted.
// On any failure (bad hex, corrupt/foreign blob) the input string is returned
// unchanged, mirroring the AutoIt's graceful degradation.
std::string unprotect(const std::string& stored);

}  // namespace amucore
