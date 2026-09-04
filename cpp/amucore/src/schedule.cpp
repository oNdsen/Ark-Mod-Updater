#include "amucore/schedule.h"

#include <cstdio>

namespace amucore {

std::string minuteStamp(int year, int month, int day, int hour, int minute) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d", year, month, day, hour, minute);
  return buf;
}

int mondayIndex(int tmWday) {
  // tm_wday: 0 = Sunday ... 6 = Saturday  ->  0 = Monday ... 6 = Sunday
  int w = ((tmWday % 7) + 7) % 7;
  return (w + 6) % 7;
}

bool scheduleDue(const Schedule& s, int mondayWday, int hour, int minute,
                 const std::string& stampNow) {
  if (!s.enabled) return false;
  if (mondayWday < 0 || mondayWday > 6) return false;
  if (((s.days >> mondayWday) & 1) == 0) return false;
  if (s.hour != hour || s.minute != minute) return false;
  if (!s.lastRun.empty() && s.lastRun == stampNow) return false;
  return true;
}

bool scheduleValid(const Schedule& s) {
  if (s.hour < 0 || s.hour > 23) return false;
  if (s.minute < 0 || s.minute > 59) return false;
  if ((s.days & kAllDays) == 0) return false;
  if (s.serverId != -1 && s.serverId <= 0) return false;
  return true;
}

}  // namespace amucore
