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

int64_t stampToMinutes(const std::string& stamp) {
  int y = 0, mo = 0, d = 0, h = 0, mi = 0;
  if (std::sscanf(stamp.c_str(), "%d-%d-%d %d:%d", &y, &mo, &d, &h, &mi) != 5) return -1;
  if (mo < 1 || mo > 12 || d < 1 || d > 31 || h < 0 || h > 23 || mi < 0 || mi > 59) return -1;
  // days since 1970-01-01 (proleptic Gregorian), civil-from-days inverse
  const int yy = mo <= 2 ? y - 1 : y;
  const int era = (yy >= 0 ? yy : yy - 399) / 400;
  const int yoe = yy - era * 400;
  const int doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  const int64_t days = static_cast<int64_t>(era) * 146097 + doe - 719468;
  return days * 1440 + h * 60 + mi;
}

bool intervalDue(const std::string& last, int intervalHours, const std::string& now) {
  if (intervalHours <= 0) return false;
  const int64_t l = stampToMinutes(last);
  if (l < 0) return true;  // never ran (or unreadable) -> due
  const int64_t n = stampToMinutes(now);
  if (n < 0) return false;
  return n - l >= static_cast<int64_t>(intervalHours) * 60;
}

bool scheduleValid(const Schedule& s) {
  if (s.hour < 0 || s.hour > 23) return false;
  if (s.minute < 0 || s.minute > 59) return false;
  if ((s.days & kAllDays) == 0) return false;
  if (s.serverId != -1 && s.serverId <= 0) return false;
  return true;
}

}  // namespace amucore
