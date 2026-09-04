#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

// World (map) backups: the server's SavedArks folder zipped into one
// timestamped archive, with rotation. Pure helpers (name, include rules,
// rotation order) are unit-tested; zipDirectory() is the only part that
// touches the file system, and it is tested through a temp directory plus the
// tiny ZIP reader in test_worldbackup.cpp.
//
// The ZIP writer is deliberately minimal and self-contained (zlib deflate, no
// third-party archive library): local headers patched after streaming, a
// central directory, and ZIP64 records whenever a size or offset does not fit
// 32 bits - an ASE map with its profiles can be several gigabytes.

namespace amucore {

// "<safe server name>_<map>_YYYY-MM-DD_HH-MM-SS.zip". Everything outside
// [A-Za-z0-9-_] in the name and map collapses to '_' so the file name is safe
// on NTFS and sorts chronologically (rotation relies on that order).
std::string backupFileName(const std::string& serverName, const std::string& map, int year,
                           int month, int day, int hour, int minute, int second);

// The prefix every backup of one server/map shares: "<safe name>_<map>_".
std::string backupPrefix(const std::string& serverName, const std::string& map);

// Which files of the SavedArks tree go into the archive. `relPath` uses '/'
// separators; `mapStem` is the map name ("TheIsland", "TheIsland_WP").
//   - *.ark: only the LIVE world file (stem == mapStem, case-insensitive) -
//     ARK's own dated copies (TheIsland_18.09.2026_04.00.00.ark) and any
//     other map's file stay out; they would multiply the archive size
//   - *.bak, *.tmp, *.part, *.amunew: never
//   - everything else (profiles, tribes, tributes, sub-folders): yes
bool includeInBackup(const std::string& relPath, const std::string& mapStem);

// The folder that holds the map's save: <installRoot>\ShooterGame\Saved\
// SavedArks, or its <map> sub-folder when that exists (ASA lays the world out
// as SavedArks\<Map_WP>\<Map_WP>.ark). Pure string logic; the caller checks
// existence.
std::string savedArksDir(const std::string& installRoot);

// PURE rotation decision: given the names of the existing backups that carry
// `prefix` (any order), return the ones to delete so that at most `keep`
// remain - the oldest first, judged by the chronological file name.
std::vector<std::string> backupsToDelete(std::vector<std::string> names, const std::string& prefix,
                                         int keep);

struct ZipResult {
  bool ok = false;
  std::string error;
  int files = 0;
  uint64_t bytesIn = 0;   // uncompressed
  uint64_t bytesOut = 0;  // archive size
};

// Zip the tree under `srcDir` (UTF-8 path) into `destZip`, applying
// includeInBackup() with `mapStem`. Entry names are the '/'-separated paths
// relative to srcDir. A failure leaves no partial archive behind.
// `forceZip64` exists for the tests only (exercises the 64-bit records on a
// tiny archive). `abort`, when set and true, stops the run between files or
// buffers ("aborted", partial archive removed) - so closing AMU never waits
// for a multi-GB archive.
ZipResult zipDirectory(const std::string& srcDir, const std::string& mapStem,
                       const std::string& destZip, bool forceZip64 = false,
                       const std::atomic<bool>* abort = nullptr);

}  // namespace amucore
