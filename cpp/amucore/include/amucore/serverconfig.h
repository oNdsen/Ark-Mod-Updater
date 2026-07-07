#pragma once

#include <string>
#include <vector>

namespace amucore {

// GameUserSettings.ini handling for the ARK server's [ServerSettings] ActiveMods
// key (a comma-separated Steam Workshop mod-id list). Mirrors amu.au3's
// IniRead/IniWrite usage. The read/write functions use the Win32 profile APIs
// (GetPrivateProfileStringW / WritePrivateProfileStringW) exactly like AutoIt's
// IniRead/IniWrite, so unrelated keys/sections are preserved byte-for-byte.
//
// The pure list helpers (parseActiveMods / joinActiveMods / addModId /
// removeModId) contain no I/O and are unit-testable without a file.

// Section/key constants used by AMU for the ActiveMods list.
inline constexpr const char* kServerSettingsSection = "ServerSettings";
inline constexpr const char* kActiveModsKey = "ActiveMods";

// Split a comma-separated ActiveMods value into ids. Each element is trimmed of
// surrounding whitespace and empty elements are dropped. Mirrors AutoIt
// StringSplit(",") + StringStripWS($STR_STRIPALL) with the empty-skip guard.
std::vector<std::string> parseActiveMods(const std::string& value);

// Join ids back into the on-disk "id1,id2,id3" form (no spaces). Empty ids are
// skipped so the result never contains stray commas.
std::string joinActiveMods(const std::vector<std::string>& ids);

// Return a copy of `ids` with `id` appended, unless an (trimmed) id equal to
// `id` is already present. `added` (if non-null) is set to true when appended,
// false when it was already active. Mirrors _AddModToServer's dedup + append.
std::vector<std::string> addModId(const std::vector<std::string>& ids,
                                  const std::string& id, bool* added = nullptr);

// Return a copy of `ids` with every (trimmed) occurrence of `id` removed and
// empty entries dropped. `removed` (if non-null) is set to true when at least
// one entry was dropped. Mirrors _RemoveSelectedMod's rebuild loop.
std::vector<std::string> removeModId(const std::vector<std::string>& ids,
                                     const std::string& id,
                                     bool* removed = nullptr);

// Read [ServerSettings] ActiveMods from `iniPath` and return the parsed ids.
// Returns an empty vector if the file/section/key is absent (like IniRead's ""
// default). `iniPath` is a UTF-8 path.
std::vector<std::string> readActiveMods(const std::string& iniPath);

// Write `ids` as the [ServerSettings] ActiveMods value in `iniPath`, preserving
// all other keys/sections. Returns false if the underlying WritePrivateProfile
// call fails. `iniPath` is a UTF-8 path.
bool writeActiveMods(const std::string& iniPath, const std::vector<std::string>& ids);

// Convenience: read ActiveMods from `iniPath`, add `id` (dedup) and write back.
// Returns true if the id was newly added (and the file written), false if it was
// already present (no write performed).
bool addActiveMod(const std::string& iniPath, const std::string& id);

// Convenience: read ActiveMods from `iniPath`, remove every occurrence of `id`
// and write back. Returns true if at least one entry was removed (and the file
// written), false if `id` was not present (a write is still performed to
// normalize the value, matching AutoIt which always IniWrites the rebuilt list).
bool removeActiveMod(const std::string& iniPath, const std::string& id);

// --- Generic INI access (Win32 profile APIs, UTF-8) -------------------------
// Mirror AutoIt IniRead/IniWrite: unrelated keys/sections are preserved. These
// back the RCON-field lookups and the full server configurator. `iniPath`,
// `section`, `key`, `value` and `def` are all UTF-8.

// Read one value; returns `def` when the file/section/key is absent.
std::string iniRead(const std::string& iniPath, const std::string& section,
                    const std::string& key, const std::string& def = "");

// Write one value (creating the section/key if needed). Returns false on failure.
bool iniWrite(const std::string& iniPath, const std::string& section,
              const std::string& key, const std::string& value);

// One "Key=Value" line of an ini section, in on-disk order.
struct IniEntry {
  std::string key;
  std::string value;
};

// One [Section] with its entries, in on-disk order.
struct IniSection {
  std::string name;
  std::vector<IniEntry> entries;
};

// Read the WHOLE ini file: every section with every entry, preserving both the
// on-disk order and DUPLICATE keys (ARK arrays like OverrideEngramEntries repeat
// the same key many times - a per-key GetPrivateProfileString read would only
// ever see the first occurrence, which is why the configurator UI fetches this
// full dump instead of doing per-key reads). Missing file -> empty vector.
// Note: the profile APIs skip ';' comment lines and strip quotes around values.
std::vector<IniSection> iniReadAll(const std::string& iniPath);

// Delete one key from a section (WritePrivateProfileStringW with a NULL value).
// ARK then falls back to its built-in default for that setting. Returns false
// on failure; deleting an already-absent key succeeds.
bool iniDeleteKey(const std::string& iniPath, const std::string& section,
                  const std::string& key);

// --- raw key-family rewrite (ARK array settings) -----------------------------
// ARK's complex Game.ini settings are ARRAYS: either the SAME key repeated on
// many lines (OverrideNamedEngramEntries=(...) x N) or an [i]-indexed family
// (PerLevelStatsMultiplier_Player[0]=..., [1]=...). The Win32 profile API holds
// ONE value per key, so these families are rewritten as raw text instead.

// PURE core (no I/O, unit-testable): within [section] (matched case-insensitive
// on the trimmed name, FIRST occurrence) remove every line whose key part (the
// text before the first '=', trimmed) equals `key` (case-insensitive) OR starts
// with `key` immediately followed by "[" (the family rule), then insert
// `fullLines` (complete "Key=..." lines, no newlines) at the first removed
// line's position - or before the next section header / at EOF when nothing
// matched; the "[section]" header is created at EOF when the section is
// missing. An empty `fullLines` removes the family. Handles CRLF and LF input
// and inserts using the file's dominant line ending; everything else stays
// byte-identical (comments, blank lines, other sections).
std::string replaceKeyLinesInText(const std::string& text, const std::string& section,
                                  const std::string& key,
                                  const std::vector<std::string>& fullLines);

// File wrapper around replaceKeyLinesInText. The file is read as raw bytes: a
// UTF-16LE BOM (FF FE) file is decoded to UTF-8 for the core and re-encoded
// UTF-16LE+BOM on write; anything else is 8-bit passthrough (UTF-8/ANSI is
// preserved byte-for-byte). A missing file is created (8-bit, CRLF). The write
// is atomic-ish: "<path>.tmp" first, then MoveFileExW REPLACE_EXISTING.
bool iniReplaceKeyLines(const std::string& iniPath, const std::string& section,
                        const std::string& key, const std::vector<std::string>& fullLines);

}  // namespace amucore
