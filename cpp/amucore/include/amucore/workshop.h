#pragma once

#include <cstdint>
#include <string>

namespace amucore {

// Result of scraping a Steam Workshop item detail page. Mirrors the four-element
// array returned by _GetModInfos in amu.au3 (name / previewUrl / date / status).
struct WorkshopModInfo {
  std::string name;        // workshopItemTitle text, or "" when unavailable
  std::string previewUrl;  // preview image URL, or "" when none was found
  std::string date;        // last-updated text (detailsStatRight), or ""
  std::string posted;      // "Posted" text - when the item was first published, or ""

  bool available = false;  // false when the item was removed / private / not found
  bool netError = false;   // true when NO response arrived (offline, WinHTTP failure) - retry later
};

// PURE parser for the HTML of
//   https://steamcommunity.com/sharedfiles/filedetails/?id=<modid>
// Extracts the mod title (class "workshopItemTitle"), the preview image URL (the
// last ShowEnlargedImagePreview('...') url, falling back to the og:image meta tag),
// and the last detailsStatRight value (last-updated date). Sets available=false when
// the page has no workshopItemTitle (removed / private / not found).
// Hand-parsed with find/substr only -- NO std::regex (heap-corrupts on MSVC 19.50).
WorkshopModInfo parseWorkshopHtml(const std::string& html);

// Manifest / size / updated pulled from steamcmd's appworkshop_346110.acf for one
// mod id. Mirrors _GetModInfosFromACF in amu.au3. Empty strings when the mod is not
// present in the ACF. Field labels reflect the actual on-disk order in the
// WorkshopItemsInstalled block ("size", "timeupdated", "manifest").
struct AcfModInfo {
  std::string size;        // "size" value (bytes on disk)
  std::string timeUpdated;  // "timeupdated" value (unix seconds)
  std::string manifest;    // "manifest" value
};

// PURE parser for appworkshop_346110.acf contents. Finds the block whose key line
// is EXACTLY "<modId>" (quoted) and reads the three following value lines
// positionally (matching the AutoIt line+2/+3/+4 + StringSplit-on-quote element
// [4] logic). Returns all empty when the mod id is not found.
// The exact match matters: a substring search would also hit another mod's
// 19-digit "manifest" or 10-digit "timeupdated" value that merely starts with
// this id, and then read size/timeupdated/manifest out of the WRONG block.
AcfModInfo parseAcfForMod(const std::string& acfText, const std::string& modId);

// Fetch the Workshop page for `modId` over HTTPS via WinHTTP and run it through
// parseWorkshopHtml. `available` is false on a network/COM failure so callers do
// NOT wrongly conclude the item was removed (matches the "neterror" branch in
// _GetModInfos). Declared here; implemented in workshop.cpp behind _WIN32.
WorkshopModInfo getModInfo(uint64_t modId);

}  // namespace amucore
