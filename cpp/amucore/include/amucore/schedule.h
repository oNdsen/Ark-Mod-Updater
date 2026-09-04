#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Scheduled update checks. At the configured time of day, on the selected
// weekdays, AMU starts an update run for one server or for all of them - the
// very same run the "Check for updates" button starts: steamcmd decides what
// is outdated, and the pre-shutdown warnings (warnplan.h) run only when
// something has to be installed under a running server. AMU itself has to be
// running; there is no service or Windows task behind this.
//
// The decision logic is pure (unit-tested); the host runs a small watcher
// thread that evaluates it every 20 s against the local clock.

namespace amucore {

struct Schedule {
  int64_t id = 0;
  bool enabled = true;
  std::string name;
  int hour = 4;
  int minute = 0;
  int days = 127;         // bit 0 = Monday ... bit 6 = Sunday; 127 = every day
  int64_t serverId = -1;  // -1 = all servers
  std::string lastRun;    // minute stamp of the last firing, "" = never
};

constexpr int kAllDays = 127;

// "YYYY-MM-DD HH:MM" for local time components (month 1-12, hour 0-23).
std::string minuteStamp(int year, int month, int day, int hour, int minute);

// tm_wday (0 = Sunday) -> the Monday-based index 0..6 used by Schedule::days.
int mondayIndex(int tmWday);

// PURE: does `s` fire at this local minute? Yes when it is enabled, its
// weekday bit is set, hour and minute match exactly, and it has not fired in
// this very minute already (lastRun != stampNow). Minute precision plus that
// guard means a watcher polling every 20-30 s can neither miss a minute nor
// fire twice.
bool scheduleDue(const Schedule& s, int mondayWday, int hour, int minute,
                 const std::string& stampNow);

// PURE: the ranges the UI save path enforces (hour 0-23, minute 0-59, at
// least one weekday, serverId -1 or > 0).
bool scheduleValid(const Schedule& s);

}  // namespace amucore
