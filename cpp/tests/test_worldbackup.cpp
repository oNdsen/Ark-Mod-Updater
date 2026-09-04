#include <doctest/doctest.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "amucore/schedule.h"
#include "amucore/worldbackup.h"
#include "zlib.h"

using namespace amucore;
namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// pure helpers

TEST_CASE("backupFileName is NTFS-safe and chronological") {
  CHECK(backupFileName("My ARK Server #1", "TheIsland", 2026, 9, 4, 4, 5, 9) ==
        "My_ARK_Server_1_TheIsland_2026-09-04_04-05-09.zip");
  CHECK(backupPrefix("My ARK Server #1", "TheIsland") == "My_ARK_Server_1_TheIsland_");
  CHECK(backupFileName("", "TheIsland_WP", 2026, 1, 1, 0, 0, 0) ==
        "server_TheIsland_WP_2026-01-01_00-00-00.zip");
  // a later stamp sorts after an earlier one as plain strings
  CHECK(backupFileName("s", "m", 2026, 9, 4, 23, 59, 59) < backupFileName("s", "m", 2026, 9, 5, 0, 0, 0));
}

TEST_CASE("includeInBackup keeps the live world, profiles and tribes, drops ARK's dated copies") {
  CHECK(includeInBackup("TheIsland.ark", "TheIsland"));
  CHECK(includeInBackup("theisland.ARK", "TheIsland"));  // case-insensitive
  CHECK_FALSE(includeInBackup("TheIsland_18.09.2026_04.00.00.ark", "TheIsland"));
  CHECK_FALSE(includeInBackup("TheIsland_ModBak_04.09.2026_18.00.00.ark", "TheIsland"));
  CHECK_FALSE(includeInBackup("Ragnarok.ark", "TheIsland"));
  CHECK(includeInBackup("76561198000000000.arkprofile", "TheIsland"));
  CHECK(includeInBackup("1234567890.arktribe", "TheIsland"));
  CHECK(includeInBackup("1234567890.arktributetribe", "TheIsland"));
  CHECK(includeInBackup("TheIsland_WP/TheIsland_WP.ark", "TheIsland_WP"));  // ASA layout
  CHECK_FALSE(includeInBackup("TheIsland.ark.bak", "TheIsland"));
  CHECK_FALSE(includeInBackup("x.tmp", "TheIsland"));
  CHECK_FALSE(includeInBackup("x.part", "TheIsland"));
  CHECK_FALSE(includeInBackup("123.amunew", "TheIsland"));
}

TEST_CASE("backupsToDelete keeps the newest N of this prefix only") {
  std::vector<std::string> names = {
      "srv_TheIsland_2026-09-03_04-00-00.zip", "srv_TheIsland_2026-09-01_04-00-00.zip",
      "srv_TheIsland_2026-09-02_04-00-00.zip", "srv_TheIsland_2026-09-04_04-00-00.zip",
      "other_TheIsland_2026-08-01_04-00-00.zip", "srv_TheIsland_notes.txt"};
  const auto del = backupsToDelete(names, "srv_TheIsland_", 2);
  REQUIRE(del.size() == 2);
  CHECK(del[0] == "srv_TheIsland_2026-09-01_04-00-00.zip");
  CHECK(del[1] == "srv_TheIsland_2026-09-02_04-00-00.zip");
  CHECK(backupsToDelete(names, "srv_TheIsland_", 10).empty());
  CHECK(backupsToDelete(names, "srv_TheIsland_", 0).size() == 3);  // keep < 1 behaves like 1
}

TEST_CASE("intervalDue and stampToMinutes") {
  CHECK(stampToMinutes("2026-09-04 04:00") - stampToMinutes("2026-09-03 04:00") == 1440);
  CHECK(stampToMinutes("2026-03-01 00:00") - stampToMinutes("2026-02-28 23:59") == 1);
  CHECK(stampToMinutes("garbage") == -1);
  CHECK(intervalDue("", 2, "2026-09-04 04:00"));                    // never ran
  CHECK_FALSE(intervalDue("", 0, "2026-09-04 04:00"));              // disabled
  CHECK_FALSE(intervalDue("2026-09-04 03:00", 2, "2026-09-04 04:59"));
  CHECK(intervalDue("2026-09-04 03:00", 2, "2026-09-04 05:00"));
  CHECK(intervalDue("2026-09-03 03:00", 24, "2026-09-04 03:00"));
}

// ---------------------------------------------------------------------------
// the archive: written by zipDirectory, read back with a minimal reader that
// understands the ZIP64 records as well

namespace {

uint16_t rd16(const std::string& b, size_t o) { return static_cast<uint16_t>(static_cast<uint8_t>(b[o]) | (static_cast<uint8_t>(b[o + 1]) << 8)); }
uint32_t rd32(const std::string& b, size_t o) { return rd16(b, o) | (static_cast<uint32_t>(rd16(b, o + 2)) << 16); }
uint64_t rd64(const std::string& b, size_t o) { return rd32(b, o) | (static_cast<uint64_t>(rd32(b, o + 4)) << 32); }

struct ReadEntry { std::string name; std::string data; };

std::vector<ReadEntry> readZip(const std::string& file, bool* zip64Seen) {
  std::ifstream in(fs::path(file), std::ios::binary);
  std::string b((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  REQUIRE(b.size() > 22);
  size_t eocd = b.size() - 22;
  REQUIRE(rd32(b, eocd) == 0x06054b50);
  uint64_t entries = rd16(b, eocd + 10);
  uint64_t cdStart = rd32(b, eocd + 16);
  *zip64Seen = false;
  if (entries == 0xFFFF || cdStart == 0xFFFFFFFFu) {
    *zip64Seen = true;
    const size_t loc = eocd - 20;
    REQUIRE(rd32(b, loc) == 0x07064b50);
    const uint64_t z64 = rd64(b, loc + 8);
    REQUIRE(rd32(b, static_cast<size_t>(z64)) == 0x06064b50);
    entries = rd64(b, static_cast<size_t>(z64) + 32);
    cdStart = rd64(b, static_cast<size_t>(z64) + 48);
  }
  std::vector<ReadEntry> out;
  size_t p = static_cast<size_t>(cdStart);
  for (uint64_t i = 0; i < entries; ++i) {
    REQUIRE(rd32(b, p) == 0x02014b50);
    const uint16_t method = rd16(b, p + 10);
    const uint32_t crc = rd32(b, p + 16);
    uint64_t csize = rd32(b, p + 20), usize = rd32(b, p + 24);
    const uint16_t nlen = rd16(b, p + 28), xlen = rd16(b, p + 30), clen = rd16(b, p + 32);
    uint64_t lho = rd32(b, p + 42);
    const std::string name = b.substr(p + 46, nlen);
    // zip64 extra
    size_t x = p + 46 + nlen;
    const size_t xend = x + xlen;
    while (x + 4 <= xend) {
      const uint16_t id = rd16(b, x), sz = rd16(b, x + 2);
      if (id == 1) {
        size_t q = x + 4;
        if (usize == 0xFFFFFFFFu) { usize = rd64(b, q); q += 8; }
        if (csize == 0xFFFFFFFFu) { csize = rd64(b, q); q += 8; }
        if (lho == 0xFFFFFFFFu) { lho = rd64(b, q); q += 8; }
        *zip64Seen = true;
      }
      x += 4 + sz;
    }
    p += 46 + nlen + xlen + clen;
    // local header
    const size_t l = static_cast<size_t>(lho);
    REQUIRE(rd32(b, l) == 0x04034b50);
    const uint16_t lnlen = rd16(b, l + 26), lxlen = rd16(b, l + 28);
    const size_t dataStart = l + 30 + lnlen + lxlen;
    REQUIRE(method == 8);
    // inflate raw
    std::string data(static_cast<size_t>(usize), '\0');
    z_stream zs{};
    REQUIRE(inflateInit2(&zs, -MAX_WBITS) == Z_OK);
    zs.next_in = reinterpret_cast<Bytef*>(&b[dataStart]);
    zs.avail_in = static_cast<uInt>(csize);
    zs.next_out = reinterpret_cast<Bytef*>(data.data());
    zs.avail_out = static_cast<uInt>(data.size());
    const int rc = inflate(&zs, Z_FINISH);
    inflateEnd(&zs);
    REQUIRE((rc == Z_STREAM_END || (rc == Z_BUF_ERROR && usize == 0)));
    CHECK(crc32(0L, reinterpret_cast<const Bytef*>(data.data()), static_cast<uInt>(data.size())) == crc);
    out.push_back(ReadEntry{name, data});
  }
  return out;
}

fs::path makeSaveTree(const std::string& tag) {
  const fs::path root = fs::temp_directory_path() / ("amu_backup_test_" + tag);
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root / "sub", ec);
  auto write = [&](const fs::path& p, const std::string& content) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f << content;
  };
  std::string big(300000, 'A');  // compressible, larger than the 1 MB buffers? no - keep it modest
  for (size_t i = 0; i < big.size(); i += 7919) big[i] = static_cast<char>('a' + (i % 26));
  write(root / "TheIsland.ark", big);
  write(root / "TheIsland_18.09.2026_04.00.00.ark", "OLD COPY - must not be archived");
  write(root / "Ragnarok.ark", "other map - must not be archived");
  write(root / "76561198000000000.arkprofile", "profile");
  write(root / "sub" / "1234.arktribe", "tribe");
  write(root / "TheIsland.ark.bak", "bak");
  write(root / "empty.arktributetribe", "");
  return root;
}

}  // namespace

TEST_CASE("zipDirectory writes an archive the reader round-trips, with the include rules applied") {
  const fs::path root = makeSaveTree("plain");
  const fs::path zip = fs::temp_directory_path() / "amu_backup_test_plain.zip";
  std::error_code ec;
  fs::remove(zip, ec);

  const ZipResult r = zipDirectory(root.string(), "TheIsland", zip.string());
  REQUIRE_MESSAGE(r.ok, r.error);
  CHECK(r.files == 4);  // TheIsland.ark, arkprofile, sub/arktribe, empty tribute
  CHECK(r.bytesIn == 300000 + 7 + 5 + 0);
  CHECK(r.bytesOut > 0);
  CHECK(r.bytesOut < 300000);  // deflate did its job on the 'A' run
  CHECK_FALSE(fs::exists(fs::path(zip.string() + ".part"), ec));

  bool z64 = false;
  const auto entries = readZip(zip.string(), &z64);
  CHECK_FALSE(z64);
  REQUIRE(entries.size() == 4);
  std::vector<std::string> names;
  for (const auto& e : entries) names.push_back(e.name);
  CHECK(std::find(names.begin(), names.end(), "TheIsland.ark") != names.end());
  CHECK(std::find(names.begin(), names.end(), "sub/1234.arktribe") != names.end());
  CHECK(std::find(names.begin(), names.end(), "Ragnarok.ark") == names.end());
  CHECK(std::find(names.begin(), names.end(), "TheIsland_18.09.2026_04.00.00.ark") == names.end());
  for (const auto& e : entries) {
    if (e.name == "TheIsland.ark") {
      CHECK(e.data.size() == 300000);
      CHECK(e.data[0] == 'a');
      CHECK(e.data[1] == 'A');
    }
    if (e.name == "sub/1234.arktribe") CHECK(e.data == "tribe");
    if (e.name == "empty.arktributetribe") CHECK(e.data.empty());
  }
  fs::remove_all(root, ec);
  fs::remove(zip, ec);
}

TEST_CASE("zipDirectory with forced ZIP64 records still round-trips") {
  const fs::path root = makeSaveTree("z64");
  const fs::path zip = fs::temp_directory_path() / "amu_backup_test_z64.zip";
  std::error_code ec;
  fs::remove(zip, ec);
  const ZipResult r = zipDirectory(root.string(), "TheIsland", zip.string(), true);
  REQUIRE_MESSAGE(r.ok, r.error);
  bool z64 = false;
  const auto entries = readZip(zip.string(), &z64);
  CHECK(z64);
  REQUIRE(entries.size() == 4);
  for (const auto& e : entries)
    if (e.name == "TheIsland.ark") CHECK(e.data.size() == 300000);
  fs::remove_all(root, ec);
  fs::remove(zip, ec);
}

TEST_CASE("zipDirectory fails cleanly when there is nothing to back up") {
  const fs::path root = fs::temp_directory_path() / "amu_backup_test_empty";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root, ec);
  const fs::path zip = fs::temp_directory_path() / "amu_backup_test_empty.zip";
  const ZipResult r = zipDirectory(root.string(), "TheIsland", zip.string());
  CHECK_FALSE(r.ok);
  CHECK_FALSE(r.error.empty());
  CHECK_FALSE(fs::exists(zip, ec));
  CHECK_FALSE(zipDirectory((root / "missing").string(), "TheIsland", zip.string()).ok);
  fs::remove_all(root, ec);
}

// ---------------------------------------------------------------------------
// the synchronous runner (backup.h): folder, naming, rotation, last_backup

#include <chrono>
#include <thread>

#include "amucore/backup.h"
#include "amucore/db.h"

TEST_CASE("runBackup archives into the default folder, rotates and stamps last_backup") {
  const fs::path root = fs::temp_directory_path() / "amu_backup_test_run";
  std::error_code ec;
  fs::remove_all(root, ec);
  const fs::path saved = root / "ShooterGame" / "Saved" / "SavedArks";
  fs::create_directories(saved, ec);
  { std::ofstream f(saved / "TheIsland.ark", std::ios::binary); f << std::string(5000, 'w'); }
  { std::ofstream f(saved / "1.arkprofile", std::ios::binary); f << "p"; }

  Db db;
  REQUIRE(db.open(":memory:"));
  Server srv;
  srv.name = "Run Test";
  srv.path = root.string();
  srv.map = "TheIsland";
  srv.id = db.upsertServer(srv);
  REQUIRE(srv.id > 0);
  Settings cfg = db.settings(srv.id);
  cfg.backupKeep = 2;

  std::vector<std::string> lines;
  auto line = [&](const std::string& t) { lines.push_back(t); };
  const BackupOutcome r1 = runBackup(db, srv, cfg, line, nullptr);
  REQUIRE_MESSAGE(r1.ok, r1.error);
  CHECK(r1.files == 2);
  CHECK(r1.deleted == 0);
  CHECK(fs::exists(fs::path(r1.file), ec));
  CHECK(fs::path(r1.file).parent_path() == root / "ShooterGame" / "Saved" / "Backups");
  CHECK(fs::path(r1.file).filename().string().rfind("Run_Test_TheIsland_", 0) == 0);
  CHECK(fs::path(r1.file).extension() == ".zip");
  CHECK_FALSE(db.settings(srv.id).lastBackup.empty());
  CHECK_FALSE(lines.empty());

  // names carry seconds - space the runs out so rotation has three distinct files
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));
  const BackupOutcome r2 = runBackup(db, srv, cfg, line, nullptr);
  REQUIRE(r2.ok);
  CHECK(r2.deleted == 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));
  const BackupOutcome r3 = runBackup(db, srv, cfg, line, nullptr);
  REQUIRE(r3.ok);
  CHECK(r3.deleted == 1);  // keep 2: the oldest went
  CHECK_FALSE(fs::exists(fs::path(r1.file), ec));
  CHECK(fs::exists(fs::path(r2.file), ec));
  CHECK(fs::exists(fs::path(r3.file), ec));

  // a custom folder is honoured (and created)
  cfg.backupDir = (root / "elsewhere").string();
  const BackupOutcome r4 = runBackup(db, srv, cfg, line, nullptr);
  REQUIRE_MESSAGE(r4.ok, r4.error);
  CHECK(fs::path(r4.file).parent_path() == root / "elsewhere");

  // no SavedArks -> clean failure, nothing written
  Server bad = srv;
  bad.path = (root / "nope").string();
  const BackupOutcome rb = runBackup(db, bad, cfg, line, nullptr);
  CHECK_FALSE(rb.ok);
  CHECK_FALSE(rb.error.empty());
  fs::remove_all(root, ec);
}
