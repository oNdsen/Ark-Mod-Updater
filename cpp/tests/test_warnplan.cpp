#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "amucore/warnplan.h"

using namespace amucore;

// ---------------------------------------------------------------------------
// storage format

TEST_CASE("warnplan round-trips through the storage format") {
  std::vector<WarnStep> plan;
  plan.push_back(WarnStep{20, "Shutdown in {min} min", true});
  plan.push_back(WarnStep{5, "Five minutes left", false});
  plan.push_back(WarnStep{0, "Going down now", true});

  const std::string text = formatWarnPlan(plan);
  CHECK(text == "20\t1\tShutdown in {min} min\n5\t0\tFive minutes left\n0\t1\tGoing down now\n");

  const auto back = parseWarnPlan(text);
  REQUIRE(back.size() == 3);
  CHECK(back[0].minutes == 20);
  CHECK(back[0].enabled);
  CHECK(back[0].text == "Shutdown in {min} min");
  CHECK(back[1].minutes == 5);
  CHECK_FALSE(back[1].enabled);
  CHECK(back[2].minutes == 0);
  CHECK(back[2].text == "Going down now");
}

TEST_CASE("formatWarnPlan strips tabs and line breaks from the text") {
  std::vector<WarnStep> plan;
  plan.push_back(WarnStep{3, "  a\tb\nc\r  ", true});
  const auto back = parseWarnPlan(formatWarnPlan(plan));
  REQUIRE(back.size() == 1);
  CHECK(back[0].text == "a b c");
}

TEST_CASE("parseWarnPlan drops malformed lines and tolerates CRLF") {
  const auto plan = parseWarnPlan("10\t1\tten\r\n\r\nnonsense\r\nx\t1\tbad minutes\r\n5\tq\tbad flag\r\n1\t1\tone\r\n");
  REQUIRE(plan.size() == 2);
  CHECK(plan[0].minutes == 10);
  CHECK(plan[0].text == "ten");
  CHECK(plan[1].minutes == 1);
  CHECK(plan[1].text == "one");
}

TEST_CASE("parseWarnPlan of an empty string is an empty plan") {
  CHECK(parseWarnPlan("").empty());
  CHECK(parseWarnPlan("\n\n").empty());
}

TEST_CASE("an empty plan is stored as a marker, not as an empty column") {
  // "" means "nothing stored" to the readers (legacy/default fallback); a user
  // who deleted every warning must get NO warnings, so the empty list has to
  // survive as something non-empty that still parses back to empty.
  const std::string stored = formatWarnPlan({});
  CHECK_FALSE(stored.empty());
  CHECK(parseWarnPlan(stored).empty());
  CHECK(parseWarnPlan("# a comment\n5\t1\tfive\n").size() == 1);
}

// ---------------------------------------------------------------------------
// defaults and the legacy migration

TEST_CASE("defaultWarnPlan is 20/15/10/5/1 minutes, all enabled, with {min}") {
  const auto plan = defaultWarnPlan();
  REQUIRE(plan.size() == 5);
  const int expected[] = {20, 15, 10, 5, 1};
  for (size_t i = 0; i < plan.size(); ++i) {
    CHECK(plan[i].minutes == expected[i]);
    CHECK(plan[i].enabled);
    CHECK(plan[i].text.find("{min}") != std::string::npos);
  }
}

TEST_CASE("legacyWarnPlan maps the AutoIt columns to restarttime / 1 / 0") {
  const auto plan = legacyWarnPlan(5, "first", "second", "third");
  REQUIRE(plan.size() == 3);
  CHECK(plan[0].minutes == 5);
  CHECK(plan[0].text == "first");
  CHECK(plan[1].minutes == 1);
  CHECK(plan[1].text == "second");
  CHECK(plan[2].minutes == 0);
  CHECK(plan[2].text == "third");
}

TEST_CASE("legacyWarnPlan skips empty messages and is empty for a fresh database") {
  const auto partial = legacyWarnPlan(10, "only first", "", "  ");
  REQUIRE(partial.size() == 1);
  CHECK(partial[0].minutes == 10);
  CHECK(legacyWarnPlan(0, "", "", "").empty());
  // restarttime 0 with a msg1 still needs a positive minute mark
  const auto zero = legacyWarnPlan(0, "x", "", "");
  REQUIRE(zero.size() == 1);
  CHECK(zero[0].minutes == 1);
}

// ---------------------------------------------------------------------------
// the countdown

TEST_CASE("countdownSteps keeps enabled steps only, sorted descending, one per minute mark") {
  std::vector<WarnStep> plan;
  plan.push_back(WarnStep{5, "five", true});
  plan.push_back(WarnStep{20, "twenty", true});
  plan.push_back(WarnStep{10, "ten (off)", false});
  plan.push_back(WarnStep{5, "five again", true});
  plan.push_back(WarnStep{-3, "negative", true});
  plan.push_back(WarnStep{1, "one", true});

  const auto steps = countdownSteps(plan);
  REQUIRE(steps.size() == 3);
  CHECK(steps[0].minutes == 20);
  CHECK(steps[1].minutes == 5);
  CHECK(steps[1].text == "five");  // the first listed wins
  CHECK(steps[2].minutes == 1);
}

TEST_CASE("countdownSteps of an all-disabled plan is empty") {
  std::vector<WarnStep> plan;
  plan.push_back(WarnStep{5, "five", false});
  CHECK(countdownSteps(plan).empty());
}

TEST_CASE("renderWarnText expands both placeholders") {
  CHECK(renderWarnText("Down in {min} min ({minutes})", 15) == "Down in 15 min (15)");
  CHECK(renderWarnText("no placeholder", 3) == "no placeholder");
}

TEST_CASE("secondsUntilNext walks 20/15/10/5/1 down to the shutdown") {
  const auto steps = countdownSteps(defaultWarnPlan());
  REQUIRE(steps.size() == 5);
  CHECK(secondsUntilNext(steps, 0) == 300);  // 20 -> 15
  CHECK(secondsUntilNext(steps, 1) == 300);  // 15 -> 10
  CHECK(secondsUntilNext(steps, 2) == 300);  // 10 -> 5
  CHECK(secondsUntilNext(steps, 3) == 240);  // 5 -> 1
  CHECK(secondsUntilNext(steps, 4) == 60);   // 1 -> shutdown
  CHECK(secondsUntilNext(steps, 5) == 0);    // out of range
  CHECK(countdownTotalSeconds(steps) == 1200);
}

TEST_CASE("a single step waits its own minutes; a 0-minute step waits nothing") {
  std::vector<WarnStep> one;
  one.push_back(WarnStep{5, "x", true});
  auto steps = countdownSteps(one);
  CHECK(secondsUntilNext(steps, 0) == 300);

  std::vector<WarnStep> zero;
  zero.push_back(WarnStep{0, "now", true});
  steps = countdownSteps(zero);
  CHECK(secondsUntilNext(steps, 0) == 0);
  CHECK(countdownTotalSeconds(steps) == 0);
  CHECK(countdownTotalSeconds({}) == 0);
}
