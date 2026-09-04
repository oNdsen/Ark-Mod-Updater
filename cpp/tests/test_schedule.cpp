#include <doctest/doctest.h>

#include <string>

#include "amucore/schedule.h"

using namespace amucore;

TEST_CASE("minuteStamp formats zero-padded local time") {
  CHECK(minuteStamp(2026, 9, 4, 4, 5) == "2026-09-04 04:05");
  CHECK(minuteStamp(2026, 12, 31, 23, 59) == "2026-12-31 23:59");
}

TEST_CASE("mondayIndex maps tm_wday (Sunday = 0) to a Monday-based index") {
  CHECK(mondayIndex(1) == 0);  // Monday
  CHECK(mondayIndex(2) == 1);
  CHECK(mondayIndex(6) == 5);  // Saturday
  CHECK(mondayIndex(0) == 6);  // Sunday
}

static Schedule daily4() {
  Schedule s;
  s.id = 1;
  s.enabled = true;
  s.name = "nightly";
  s.hour = 4;
  s.minute = 0;
  s.days = kAllDays;
  s.serverId = -1;
  return s;
}

TEST_CASE("scheduleDue fires exactly at its minute on an enabled weekday") {
  const Schedule s = daily4();
  CHECK(scheduleDue(s, 0, 4, 0, "2026-09-07 04:00"));
  CHECK_FALSE(scheduleDue(s, 0, 4, 1, "2026-09-07 04:01"));
  CHECK_FALSE(scheduleDue(s, 0, 3, 59, "2026-09-07 03:59"));
  CHECK_FALSE(scheduleDue(s, 0, 16, 0, "2026-09-07 16:00"));
}

TEST_CASE("scheduleDue honours the weekday mask") {
  Schedule s = daily4();
  s.days = (1 << 0) | (1 << 4);  // Monday + Friday only
  CHECK(scheduleDue(s, 0, 4, 0, "x"));
  CHECK(scheduleDue(s, 4, 4, 0, "x"));
  CHECK_FALSE(scheduleDue(s, 1, 4, 0, "x"));
  CHECK_FALSE(scheduleDue(s, 6, 4, 0, "x"));
}

TEST_CASE("scheduleDue never fires twice in the same minute and never when disabled") {
  Schedule s = daily4();
  s.lastRun = "2026-09-07 04:00";
  CHECK_FALSE(scheduleDue(s, 0, 4, 0, "2026-09-07 04:00"));  // already fired this minute
  CHECK(scheduleDue(s, 0, 4, 0, "2026-09-08 04:00"));        // next day is fine
  s.lastRun.clear();
  s.enabled = false;
  CHECK_FALSE(scheduleDue(s, 0, 4, 0, "2026-09-07 04:00"));
}

TEST_CASE("scheduleDue rejects an out-of-range weekday index") {
  const Schedule s = daily4();
  CHECK_FALSE(scheduleDue(s, -1, 4, 0, "x"));
  CHECK_FALSE(scheduleDue(s, 7, 4, 0, "x"));
}

TEST_CASE("scheduleValid enforces the field ranges") {
  Schedule s = daily4();
  CHECK(scheduleValid(s));
  s.hour = 24;
  CHECK_FALSE(scheduleValid(s));
  s = daily4();
  s.minute = 60;
  CHECK_FALSE(scheduleValid(s));
  s = daily4();
  s.days = 0;
  CHECK_FALSE(scheduleValid(s));
  s = daily4();
  s.serverId = 0;
  CHECK_FALSE(scheduleValid(s));
  s.serverId = 7;
  CHECK(scheduleValid(s));
}
