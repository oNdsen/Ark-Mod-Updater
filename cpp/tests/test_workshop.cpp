#include <doctest/doctest.h>

#include <fstream>
#include <iterator>
#include <string>

#include "amucore/workshop.h"

using namespace amucore;

static std::string readText(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(f)),
                     std::istreambuf_iterator<char>());
}

TEST_CASE("parseWorkshopHtml extracts name and preview from an available item") {
  const std::string dir = AMU_FIXTURES_DIR;
  const auto html = readText(dir + "/workshop_available.html");
  REQUIRE(!html.empty());

  const auto info = parseWorkshopHtml(html);
  CHECK(info.available == true);
  CHECK(info.name == "Structures Plus (S+)");
  // Prefers the last ShowEnlargedImagePreview url.
  CHECK(info.previewUrl ==
        "https://steamuserimages-a.akamaihd.net/ugc/preview_second.jpg");
  CHECK(info.date == "5 Nov, 2018 @ 3:14pm");
  CHECK(info.posted == "4 Nov, 2018 @ 9:00am");  // label-paired, not last-of-all

}

TEST_CASE("parseWorkshopHtml falls back to og:image when no enlarged preview") {
  const std::string html =
      "<html><head>"
      "<meta property=\"og:image\" "
      "content=\"https://cdn.example/og_preview.png\">"
      "</head><body>"
      "<div class=\"workshopItemTitle\">Just A Title</div>"
      "</body></html>";
  const auto info = parseWorkshopHtml(html);
  CHECK(info.available == true);
  CHECK(info.name == "Just A Title");
  CHECK(info.previewUrl == "https://cdn.example/og_preview.png");
}

TEST_CASE("parseWorkshopHtml marks a removed item unavailable") {
  const std::string dir = AMU_FIXTURES_DIR;
  const auto html = readText(dir + "/workshop_removed.html");
  REQUIRE(!html.empty());

  const auto info = parseWorkshopHtml(html);
  CHECK(info.available == false);
  CHECK(info.name.empty());
  CHECK(info.previewUrl.empty());
}

TEST_CASE("parseAcfForMod reads size/timeupdated/manifest for a present mod") {
  const std::string dir = AMU_FIXTURES_DIR;
  const auto acf = readText(dir + "/appworkshop_346110.acf");
  REQUIRE(!acf.empty());

  const auto a = parseAcfForMod(acf, "632091170");
  CHECK(a.size == "127868018");
  CHECK(a.timeUpdated == "1537158323");
  CHECK(a.manifest == "881726055709807503");
}

TEST_CASE("parseAcfForMod returns empty for a mod not in the ACF") {
  const std::string dir = AMU_FIXTURES_DIR;
  const auto acf = readText(dir + "/appworkshop_346110.acf");
  const auto a = parseAcfForMod(acf, "999999999");
  CHECK(a.size.empty());
  CHECK(a.timeUpdated.empty());
  CHECK(a.manifest.empty());
}

// An .acf where the FIRST line containing "632091170" belongs to a different mod:
// its 19-digit manifest value happens to start with those digits. The old
// substring search locked onto that line and read size/timeupdated/manifest from
// the wrong block.
static const char kAcfWithDecoy[] =
    "\"AppWorkshop\"\n"
    "{\n"
    "\t\"appid\"\t\t\"346110\"\n"
    "\t\"WorkshopItemsInstalled\"\n"
    "\t{\n"
    "\t\t\"893735676\"\n"
    "\t\t{\n"
    "\t\t\t\"size\"\t\t\"512\"\n"
    "\t\t\t\"timeupdated\"\t\t\"1500000000\"\n"
    "\t\t\t\"manifest\"\t\t\"6320911704242424242\"\n"
    "\t\t}\n"
    "\t\t\"632091170\"\n"
    "\t\t{\n"
    "\t\t\t\"size\"\t\t\"127868018\"\n"
    "\t\t\t\"timeupdated\"\t\t\"1537158323\"\n"
    "\t\t\t\"manifest\"\t\t\"881726055709807503\"\n"
    "\t\t}\n"
    "\t}\n"
    "}\n";

TEST_CASE("parseAcfForMod matches the block header, not another mod's value") {
  const auto a = parseAcfForMod(kAcfWithDecoy, "632091170");
  CHECK(a.size == "127868018");
  CHECK(a.timeUpdated == "1537158323");
  CHECK(a.manifest == "881726055709807503");
}

TEST_CASE("parseAcfForMod ignores a mod id that only occurs inside a value") {
  // The searched id appears only inside another mod's manifest value and has no
  // block of its own. The old substring search matched that value line and then
  // read the three "following" lines out of the NEXT mod's block, reporting
  // "999" as this mod's manifest.
  const std::string acf =
      "\"AppWorkshop\"\n"
      "{\n"
      "\t\"WorkshopItemsInstalled\"\n"
      "\t{\n"
      "\t\t\"893735676\"\n"
      "\t\t{\n"
      "\t\t\t\"size\"\t\t\"512\"\n"
      "\t\t\t\"timeupdated\"\t\t\"1500000000\"\n"
      "\t\t\t\"manifest\"\t\t\"6320911704242424242\"\n"
      "\t\t}\n"
      "\t\t\"111222333\"\n"
      "\t\t{\n"
      "\t\t\t\"size\"\t\t\"999\"\n"
      "\t\t\t\"timeupdated\"\t\t\"1\"\n"
      "\t\t\t\"manifest\"\t\t\"2\"\n"
      "\t\t}\n"
      "\t}\n"
      "}\n";

  const auto a = parseAcfForMod(acf, "632091170");
  CHECK(a.size.empty());
  CHECK(a.timeUpdated.empty());
  CHECK(a.manifest.empty());
}

TEST_CASE("parseAcfForMod tolerates CRLF line endings") {
  std::string acf;
  for (const char* p = kAcfWithDecoy; *p; ++p) {
    if (*p == '\n') acf.push_back('\r');
    acf.push_back(*p);
  }
  const auto a = parseAcfForMod(acf, "632091170");
  CHECK(a.size == "127868018");
  CHECK(a.manifest == "881726055709807503");
}

TEST_CASE("parseWorkshopHtml pairs Posted by column despite the live page's trailing label space") {
  // Captured from the live item page 2026-09-04: every detailsStatLeft label
  // carries a trailing space ("Posted </div>"); the 2018 fixture has none.
  // An exact compare returned "" and the UI showed "Released -" for every mod.
  const std::string html =
      "<div class=\"workshopItemTitle\">PrimalStacks 100K</div>"
      "<div class=\"detailsStatLeft\">File Size </div>"
      "<div class=\"detailsStatLeft\">Posted </div>"
      "<div class=\"detailsStatLeft\">Updated </div>"
      "<div class=\"detailsStatRight\">1.960 MB</div>"
      "<div class=\"detailsStatRight\">1 Jun @ 7:46am</div>"
      "<div class=\"detailsStatRight\">9 Jul @ 5:15pm</div>";
  const auto info = parseWorkshopHtml(html);
  REQUIRE(info.available);
  CHECK(info.posted == "1 Jun @ 7:46am");
  CHECK(info.date == "9 Jul @ 5:15pm");
}

TEST_CASE("parseWorkshopHtml leaves posted empty when the stats block has no Posted label") {
  const std::string html =
      "<div class=\"workshopItemTitle\">X</div>"
      "<div class=\"detailsStatLeft\">File Size</div>"
      "<div class=\"detailsStatRight\">1 MB</div>";
  const auto info = parseWorkshopHtml(html);
  REQUIRE(info.available);
  CHECK(info.posted.empty());
}
