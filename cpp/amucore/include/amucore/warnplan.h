#pragma once

#include <cstddef>
#include <string>
#include <vector>

// The pre-shutdown warning plan: what the players are told - via RCON
// broadcast - before a server is stopped for a mod update. Replaces the fixed
// AutoIt trio (msg1 at T-restarttime, msg2 at T-1 min, msg3 at T-0) with a
// per-server LIST the user edits freely: any number of steps, each with its
// own minute mark, text and enable flag. No step is implied: an empty (or
// fully disabled) plan means the server is stopped without any warning.

namespace amucore {

struct WarnStep {
  int minutes = 0;   // minutes before the shutdown; 0 = right before the stop
  std::string text;  // "{min}" / "{minutes}" expand to `minutes`
  bool enabled = true;
};

// Storage format (settings.warnplan): one step per line,
//   <minutes>\t<0|1>\t<text>
// Text may not contain tabs or line breaks (formatWarnPlan strips them).
// Malformed lines are dropped. An EMPTY column means "no plan stored" - the
// callers then fall back to legacyWarnPlan() and finally defaultWarnPlan().
std::vector<WarnStep> parseWarnPlan(const std::string& text);
std::string formatWarnPlan(const std::vector<WarnStep>& plan);

// 20 / 15 / 10 / 5 / 1 minutes, "Server shutdown in {min} min for mod updates".
std::vector<WarnStep> defaultWarnPlan();

// The plan equivalent to the AutoIt columns of an existing installation:
// msg1 at `restarttime`, msg2 at 1 minute, msg3 at 0 - empty texts skipped.
// Empty when nothing is set (a fresh AMU 2 database).
std::vector<WarnStep> legacyWarnPlan(int restarttime, const std::string& msg1,
                                     const std::string& msg2, const std::string& msg3);

// The steps the countdown actually runs: enabled only, minutes >= 0, sorted by
// minutes descending (stable). Equal minute marks stay and are sent back to
// back - secondsUntilNext() is 0 between them.
std::vector<WarnStep> countdownSteps(const std::vector<WarnStep>& plan);

// "{min}" and "{minutes}" -> the number.
std::string renderWarnText(const std::string& text, int minutes);

// Seconds to wait AFTER sending steps[i] until the next event - the next step,
// or the shutdown itself when i is the last step:
//   (steps[i].minutes - steps[i+1].minutes) * 60, or steps[i].minutes * 60.
// `steps` must come from countdownSteps() (descending, unique).
int secondsUntilNext(const std::vector<WarnStep>& steps, size_t i);

// Total countdown length in seconds (= first step's minutes * 60), 0 when empty.
int countdownTotalSeconds(const std::vector<WarnStep>& steps);

}  // namespace amucore
