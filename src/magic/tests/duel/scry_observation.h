#ifndef DUEL_SCRY_OBSERVATION_H
#define DUEL_SCRY_OBSERVATION_H
#include <algorithm>
#include <vector>

// Oracle permits any subset of the inspected cards to go to the bottom, with
// arbitrary order in both groups. The uninspected suffix must stay together.
inline bool valid_scry_result(const std::vector<int>& before,
                              const std::vector<int>& after, int amount) {
  if (amount < 0 || before.size() != after.size()) return false;
  const auto viewed = (std::min)(before.size(), static_cast<std::size_t>(amount));
  const auto tail_size = before.size() - viewed;
  std::vector<int> expected(before.begin(), before.begin() + viewed);
  std::sort(expected.begin(), expected.end());
  for (std::size_t top = 0; top <= viewed; ++top) {
    if (!std::equal(before.begin() + viewed, before.end(), after.begin() + top)) continue;
    std::vector<int> observed(after.begin(), after.begin() + top);
    observed.insert(observed.end(), after.begin() + top + tail_size, after.end());
    std::sort(observed.begin(), observed.end());
    if (observed == expected) return true;
  }
  return false;
}
#endif
