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
