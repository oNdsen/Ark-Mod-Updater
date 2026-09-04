#include "amucore/worldbackup.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>

#include "zlib.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace amucore {

namespace {

namespace fs = std::filesystem;

std::string lowerAscii(std::string s) {
  for (char& c : s)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return s;
}

std::string safeName(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (unsigned char c : s) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                    c == '-' || c == '_';
    out += ok ? static_cast<char>(c) : '_';
  }
  // collapse runs of '_' so "My  Server" -> "My_Server", not "My__Server"
  std::string collapsed;
  for (char c : out)
    if (!(c == '_' && !collapsed.empty() && collapsed.back() == '_')) collapsed += c;
  while (!collapsed.empty() && collapsed.back() == '_') collapsed.pop_back();
  return collapsed.empty() ? std::string("server") : collapsed;
}

fs::path widePath(const std::string& utf8) {
#ifdef _WIN32
  if (utf8.empty()) return fs::path();
  const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()),
                                    nullptr, 0);
  std::wstring w(static_cast<size_t>(n > 0 ? n : 0), L'\0');
  if (n > 0)
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), w.data(), n);
  return fs::path(w);
#else
  return fs::path(utf8);
#endif
}

std::string toUtf8(const fs::path& p) {
#ifdef _WIN32
  const std::wstring w = p.wstring();
  if (w.empty()) return {};
  const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0,
                                    nullptr, nullptr);
  std::string out(static_cast<size_t>(n > 0 ? n : 0), '\0');
  if (n > 0)
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), out.data(), n, nullptr,
                        nullptr);
  return out;
#else
  return p.string();
#endif
}

// --- little-endian writers -------------------------------------------------

void put16(std::string& b, uint32_t v) {
  b += static_cast<char>(v & 0xFF);
  b += static_cast<char>((v >> 8) & 0xFF);
}
void put32(std::string& b, uint32_t v) {
  put16(b, v & 0xFFFF);
  put16(b, (v >> 16) & 0xFFFF);
}
void put64(std::string& b, uint64_t v) {
  put32(b, static_cast<uint32_t>(v & 0xFFFFFFFFu));
  put32(b, static_cast<uint32_t>(v >> 32));
}

// DOS date/time of a file's last write, local time (ZIP has no zone).
void dosTime(const fs::path& p, uint16_t* dosT, uint16_t* dosD) {
  std::time_t t = std::time(nullptr);
  std::error_code ec;
  const auto ft = fs::last_write_time(p, ec);
  if (!ec) {
    const auto sys = std::chrono::clock_cast<std::chrono::system_clock>(ft);
    t = std::chrono::system_clock::to_time_t(sys);
  }
  std::tm tmv{};
#ifdef _WIN32
  localtime_s(&tmv, &t);
#else
  localtime_r(&t, &tmv);
#endif
  int year = tmv.tm_year + 1900;
  if (year < 1980) year = 1980;  // DOS epoch
  *dosT = static_cast<uint16_t>((tmv.tm_hour << 11) | (tmv.tm_min << 5) | (tmv.tm_sec / 2));
  *dosD = static_cast<uint16_t>(((year - 1980) << 9) | ((tmv.tm_mon + 1) << 5) | tmv.tm_mday);
}

constexpr uint32_t kMax32 = 0xFFFFFFFFu;
constexpr uint16_t kMax16 = 0xFFFF;
// Below this an uncompressed size gets a plain local header; above it the
// header carries a ZIP64 extra field up front, because the compressed size is
// only known after streaming and deflate can expand a little (5 bytes per 16 KB
// block, far below the 256 MB margin).
constexpr uint64_t kZip64LocalThreshold = 0xFFFFFFFFull - (256ull << 20);

struct Entry {
  std::string name;  // relative, '/'
  uint64_t offset = 0;  // local header offset
  uint64_t usize = 0;
  uint64_t csize = 0;
  uint32_t crc = 0;
  uint16_t dosT = 0, dosD = 0;
  bool zip64Local = false;
};

}  // namespace

// ---------------------------------------------------------------------------

std::string backupPrefix(const std::string& serverName, const std::string& map) {
  return safeName(serverName) + "_" + safeName(map) + "_";
}

std::string backupFileName(const std::string& serverName, const std::string& map, int year,
                           int month, int day, int hour, int minute, int second) {
  char stamp[32];
  std::snprintf(stamp, sizeof(stamp), "%04d-%02d-%02d_%02d-%02d-%02d", year, month, day, hour,
                minute, second);
  return backupPrefix(serverName, map) + stamp + ".zip";
}

bool includeInBackup(const std::string& relPath, const std::string& mapStem) {
  const size_t slash = relPath.find_last_of('/');
  const std::string file = slash == std::string::npos ? relPath : relPath.substr(slash + 1);
  const size_t dot = file.find_last_of('.');
  const std::string ext = dot == std::string::npos ? std::string() : lowerAscii(file.substr(dot + 1));
  const std::string stem = dot == std::string::npos ? file : file.substr(0, dot);
  if (ext == "bak" || ext == "tmp" || ext == "part" || ext == "amunew") return false;
  if (ext == "ark") return lowerAscii(stem) == lowerAscii(mapStem);
  return true;
}

std::string savedArksDir(const std::string& installRoot) {
  return installRoot + "\\ShooterGame\\Saved\\SavedArks";
}

std::vector<std::string> backupsToDelete(std::vector<std::string> names, const std::string& prefix,
                                         int keep) {
  std::vector<std::string> mine;
  for (const std::string& n : names)
    if (n.size() > prefix.size() && n.compare(0, prefix.size(), prefix) == 0 &&
        lowerAscii(n).size() >= 4 && lowerAscii(n).compare(n.size() - 4, 4, ".zip") == 0)
      mine.push_back(n);
  std::sort(mine.begin(), mine.end());  // the stamp makes the name chronological
  if (keep < 1) keep = 1;
  std::vector<std::string> out;
  while (mine.size() > static_cast<size_t>(keep)) {
    out.push_back(mine.front());
    mine.erase(mine.begin());
  }
  return out;
}

ZipResult zipDirectory(const std::string& srcDir, const std::string& mapStem,
                       const std::string& destZip, bool forceZip64,
                       const std::atomic<bool>* abort) {
  ZipResult res;
  std::error_code ec;
  const fs::path root = widePath(srcDir);
  if (!fs::is_directory(root, ec)) {
    res.error = "save folder not found: " + srcDir;
    return res;
  }

  // Collect first (sorted, deterministic archive), then stream.
  std::vector<fs::path> files;
  for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
       it != end; it.increment(ec)) {
    if (ec) break;
    if (!it->is_regular_file(ec)) continue;
    std::string rel = toUtf8(fs::relative(it->path(), root, ec));
    std::replace(rel.begin(), rel.end(), '\\', '/');
    if (includeInBackup(rel, mapStem)) files.push_back(it->path());
  }
  std::sort(files.begin(), files.end());
  if (files.empty()) {
    res.error = "nothing to back up in " + srcDir + " (no " + mapStem + ".ark?)";
    return res;
  }

  const fs::path dest = widePath(destZip);
  const fs::path part = widePath(destZip + ".part");
  fs::create_directories(dest.parent_path(), ec);
  std::fstream out(part, std::ios::binary | std::ios::out | std::ios::trunc);
  if (!out) {
    res.error = "cannot create " + destZip;
    return res;
  }

  std::vector<Entry> entries;
  std::vector<char> inBuf(1 << 20), outBuf(1 << 20);
  uint64_t pos = 0;
  bool failed = false;
  std::string failMsg;

  for (const fs::path& p : files) {
    if (abort && abort->load()) { failed = true; failMsg = "aborted"; break; }
    Entry e;
    std::string rel = toUtf8(fs::relative(p, root, ec));
    std::replace(rel.begin(), rel.end(), '\\', '/');
    e.name = rel;
    e.offset = pos;
    e.usize = static_cast<uint64_t>(fs::file_size(p, ec));
    dosTime(p, &e.dosT, &e.dosD);
    e.zip64Local = forceZip64 || e.usize >= kZip64LocalThreshold;

    // local header (sizes/crc patched after streaming)
    std::string hdr;
    put32(hdr, 0x04034b50);
    put16(hdr, e.zip64Local ? 45 : 20);
    put16(hdr, 0x0800);  // UTF-8 names
    put16(hdr, 8);       // deflate
    put16(hdr, e.dosT);
    put16(hdr, e.dosD);
    put32(hdr, 0);       // crc
    put32(hdr, e.zip64Local ? kMax32 : 0);  // csize
    put32(hdr, e.zip64Local ? kMax32 : 0);  // usize
    put16(hdr, static_cast<uint32_t>(e.name.size()));
    put16(hdr, e.zip64Local ? 20 : 0);      // extra length
    hdr += e.name;
    if (e.zip64Local) {
      put16(hdr, 0x0001);
      put16(hdr, 16);
      put64(hdr, 0);  // usize placeholder
      put64(hdr, 0);  // csize placeholder
    }
    out.write(hdr.data(), static_cast<std::streamsize>(hdr.size()));
    pos += hdr.size();

    // stream: raw deflate + crc
    std::ifstream in(p, std::ios::binary);
    if (!in) { failed = true; failMsg = "cannot read " + rel; break; }
    z_stream zs{};
    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
      failed = true; failMsg = "deflateInit failed"; break;
    }
    uint32_t crc = crc32(0L, Z_NULL, 0);
    uint64_t csize = 0;
    int flush = Z_NO_FLUSH;
    do {
      in.read(inBuf.data(), static_cast<std::streamsize>(inBuf.size()));
      const std::streamsize got = in.gcount();
      if (abort && abort->load()) { failed = true; failMsg = "aborted"; break; }
      flush = in.eof() ? Z_FINISH : Z_NO_FLUSH;
      crc = crc32(crc, reinterpret_cast<const Bytef*>(inBuf.data()), static_cast<uInt>(got));
      zs.next_in = reinterpret_cast<Bytef*>(inBuf.data());
      zs.avail_in = static_cast<uInt>(got);
      do {
        zs.next_out = reinterpret_cast<Bytef*>(outBuf.data());
        zs.avail_out = static_cast<uInt>(outBuf.size());
        const int rc = deflate(&zs, flush);
        if (rc == Z_STREAM_ERROR) { failed = true; failMsg = "deflate error in " + rel; break; }
        const size_t have = outBuf.size() - zs.avail_out;
        out.write(outBuf.data(), static_cast<std::streamsize>(have));
        csize += have;
      } while (zs.avail_out == 0 && !failed);
    } while (flush != Z_FINISH && !failed);
    deflateEnd(&zs);
    if (failed) break;
    if (!out) { failed = true; failMsg = "write error (disk full?)"; break; }
    e.crc = crc;
    e.csize = csize;
    pos += csize;

    // patch the local header
    const std::streampos back = out.tellp();
    out.seekp(static_cast<std::streamoff>(e.offset + 14));
    std::string patch;
    put32(patch, e.crc);
    if (e.zip64Local) {
      put32(patch, kMax32);
      put32(patch, kMax32);
    } else {
      put32(patch, static_cast<uint32_t>(e.csize));
      put32(patch, static_cast<uint32_t>(e.usize));
    }
    out.write(patch.data(), static_cast<std::streamsize>(patch.size()));
    if (e.zip64Local) {
      out.seekp(static_cast<std::streamoff>(e.offset + 30 + e.name.size() + 4));
      std::string z;
      put64(z, e.usize);
      put64(z, e.csize);
      out.write(z.data(), static_cast<std::streamsize>(z.size()));
    }
    out.seekp(back);
    res.files++;
    res.bytesIn += e.usize;
    entries.push_back(std::move(e));
  }

  if (!failed) {
    // central directory
    const uint64_t cdStart = pos;
    for (const Entry& e : entries) {
      const bool z64 = forceZip64 || e.usize >= kMax32 || e.csize >= kMax32 || e.offset >= kMax32;
      std::string extra;
      if (z64) {
        put16(extra, 0x0001);
        put16(extra, 24);
        put64(extra, e.usize);
        put64(extra, e.csize);
        put64(extra, e.offset);
      }
      std::string c;
      put32(c, 0x02014b50);
      put16(c, z64 ? 45 : 20);  // made by
      put16(c, z64 ? 45 : 20);  // needed
      put16(c, 0x0800);
      put16(c, 8);
      put16(c, e.dosT);
      put16(c, e.dosD);
      put32(c, e.crc);
      put32(c, z64 ? kMax32 : static_cast<uint32_t>(e.csize));
      put32(c, z64 ? kMax32 : static_cast<uint32_t>(e.usize));
      put16(c, static_cast<uint32_t>(e.name.size()));
      put16(c, static_cast<uint32_t>(extra.size()));
      put16(c, 0);  // comment
      put16(c, 0);  // disk
      put16(c, 0);  // internal attrs
      put32(c, 0x20);  // external: archive bit
      put32(c, z64 ? kMax32 : static_cast<uint32_t>(e.offset));
      c += e.name;
      c += extra;
      out.write(c.data(), static_cast<std::streamsize>(c.size()));
      pos += c.size();
    }
    const uint64_t cdSize = pos - cdStart;
    const bool z64eocd = forceZip64 || entries.size() >= kMax16 || cdSize >= kMax32 || cdStart >= kMax32;
    if (z64eocd) {
      const uint64_t z64Pos = pos;
      std::string r;
      put32(r, 0x06064b50);
      put64(r, 44);  // size of the remainder
      put16(r, 45);
      put16(r, 45);
      put32(r, 0);
      put32(r, 0);
      put64(r, entries.size());
      put64(r, entries.size());
      put64(r, cdSize);
      put64(r, cdStart);
      out.write(r.data(), static_cast<std::streamsize>(r.size()));
      pos += r.size();
      std::string l;
      put32(l, 0x07064b50);
      put32(l, 0);
      put64(l, z64Pos);
      put32(l, 1);
      out.write(l.data(), static_cast<std::streamsize>(l.size()));
      pos += l.size();
    }
    std::string eocd;
    put32(eocd, 0x06054b50);
    put16(eocd, 0);
    put16(eocd, 0);
    put16(eocd, z64eocd ? kMax16 : static_cast<uint32_t>(entries.size()));
    put16(eocd, z64eocd ? kMax16 : static_cast<uint32_t>(entries.size()));
    put32(eocd, z64eocd ? kMax32 : static_cast<uint32_t>(cdSize));
    put32(eocd, z64eocd ? kMax32 : static_cast<uint32_t>(cdStart));
    put16(eocd, 0);
    out.write(eocd.data(), static_cast<std::streamsize>(eocd.size()));
    pos += eocd.size();
    if (!out) { failed = true; failMsg = "write error (disk full?)"; }
  }
  out.close();

  if (failed) {
    fs::remove(part, ec);
    res.error = failMsg;
    return res;
  }
  fs::remove(dest, ec);
  fs::rename(part, dest, ec);
  if (ec) {
    fs::remove(part, ec);
    res.error = "could not move the archive into place: " + destZip;
    return res;
  }
  res.ok = true;
  res.bytesOut = pos;
  return res;
}

}  // namespace amucore
