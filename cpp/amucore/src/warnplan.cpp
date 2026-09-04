#include "amucore/warnplan.h"

#include <algorithm>
#include <cstdlib>

namespace amucore {

namespace {

constexpr size_t kNpos = std::string::npos;

// Trim spaces and CR from both ends (the storage lines may end in \r\n).
std::string trim(const std::string& s) {
  size_t b = 0;
  size_t e = s.size();
  while (b < e && (s[b] == ' ' || s[b] == '\r')) ++b;
  while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\r')) --e;
  return s.substr(b, e - b);
}

// Strict small-int parse: "" / non-digits / > 6 digits -> false.
bool parseInt(const std::string& s, int* out) {
  const std::string t = trim(s);
  if (t.empty() || t.size() > 6) return false;
  size_t i = 0;
  bool neg = false;
  if (t[0] == '-') { neg = true; i = 1; }
  if (i >= t.size()) return false;
  int v = 0;
  for (; i < t.size(); ++i) {
    if (t[i] < '0' || t[i] > '9') return false;
    v = v * 10 + (t[i] - '0');
  }
  *out = neg ? -v : v;
  return true;
}

// The text column: no tabs or line breaks (they are the record separators).
std::string sanitizeText(const std::string& text) {
  std::string out;
  out.reserve(text.size());
  for (char c : text) {
    if (c == '\t' || c == '\n' || c == '\r') out += ' ';
    else out += c;
  }
  return trim(out);
}

std::string replaceAll(std::string s, const std::string& from, const std::string& to) {
  if (from.empty()) return s;
  size_t pos = 0;
  while ((pos = s.find(from, pos)) != kNpos) {
    s.replace(pos, from.size(), to);
    pos += to.size();
  }
  return s;
}

}  // namespace

std::vector<WarnStep> parseWarnPlan(const std::string& text) {
  std::vector<WarnStep> out;
  size_t start = 0;
  while (start <= text.size()) {
    size_t nl = text.find('\n', start);
    const std::string line = text.substr(start, nl == kNpos ? kNpos : nl - start);
    if (!trim(line).empty() && trim(line)[0] != '#') {  // '#' = comment / marker line
      // <minutes> \t <enabled> \t <text>  - text may contain further tabs? No:
      // sanitizeText removed them on write, but be lenient and keep the rest.
      const size_t t1 = line.find('\t');
      const size_t t2 = t1 == kNpos ? kNpos : line.find('\t', t1 + 1);
      int minutes = 0, enabled = 1;
      if (t1 != kNpos && t2 != kNpos && parseInt(line.substr(0, t1), &minutes) &&
          parseInt(line.substr(t1 + 1, t2 - t1 - 1), &enabled)) {
        WarnStep st;
        st.minutes = minutes;
        st.enabled = enabled != 0;
        st.text = sanitizeText(line.substr(t2 + 1));
        out.push_back(std::move(st));
      }
      // malformed lines are dropped by design
    }
    if (nl == kNpos) break;
    start = nl + 1;
  }
  return out;
}

std::string formatWarnPlan(const std::vector<WarnStep>& plan) {
  // An EMPTY plan is a deliberate "stop without any warning" and must survive
  // as such: the readers treat an empty column as "nothing stored" and fall
  // back to the legacy/default plans, so the empty list is stored as a
  // comment-only marker that parses back to an empty plan.
  if (plan.empty()) return "#none\n";
  std::string out;
  for (const WarnStep& st : plan) {
    out += std::to_string(st.minutes);
    out += '\t';
    out += st.enabled ? '1' : '0';
    out += '\t';
    out += sanitizeText(st.text);
    out += '\n';
  }
  return out;
}

std::vector<WarnStep> defaultWarnPlan() {
  std::vector<WarnStep> out;
  for (int m : {20, 15, 10, 5, 1}) {
    WarnStep st;
    st.minutes = m;
    st.text = "Server shutdown in {min} min for mod updates";
    st.enabled = true;
    out.push_back(std::move(st));
  }
  return out;
}

std::vector<WarnStep> legacyWarnPlan(int restarttime, const std::string& msg1,
                                     const std::string& msg2, const std::string& msg3) {
  std::vector<WarnStep> out;
  auto add = [&out](int minutes, const std::string& text) {
    if (trim(text).empty()) return;
    WarnStep st;
    st.minutes = minutes;
    st.text = sanitizeText(text);
    st.enabled = true;
    out.push_back(std::move(st));
  };
  // The AutoIt sent msg1, waited restarttime-1 minutes, sent msg2, waited one
  // minute, sent msg3 and stopped. Minute marks: restarttime / 1 / 0.
  add(restarttime > 0 ? restarttime : 1, msg1);
  add(1, msg2);
  add(0, msg3);
  return out;
}

std::vector<WarnStep> countdownSteps(const std::vector<WarnStep>& plan) {
  std::vector<WarnStep> out;
  for (const WarnStep& st : plan) {
    if (!st.enabled || st.minutes < 0) continue;
    bool dup = false;
    for (const WarnStep& have : out)
      if (have.minutes == st.minutes) { dup = true; break; }
    if (!dup) out.push_back(st);
  }
  std::stable_sort(out.begin(), out.end(),
                   [](const WarnStep& a, const WarnStep& b) { return a.minutes > b.minutes; });
  return out;
}

std::string renderWarnText(const std::string& text, int minutes) {
  const std::string n = std::to_string(minutes);
  return replaceAll(replaceAll(text, "{minutes}", n), "{min}", n);
}

int secondsUntilNext(const std::vector<WarnStep>& steps, size_t i) {
  if (i >= steps.size()) return 0;
  const int next = (i + 1 < steps.size()) ? steps[i + 1].minutes : 0;
  const int delta = steps[i].minutes - next;
  return delta > 0 ? delta * 60 : 0;
}

int countdownTotalSeconds(const std::vector<WarnStep>& steps) {
  return steps.empty() ? 0 : (steps.front().minutes > 0 ? steps.front().minutes * 60 : 0);
}

}  // namespace amucore
