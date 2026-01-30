#include <string>
#include <vector>
#include <variant>

template <class Tag> struct StrongString {
  std::string stat;
};

using Date = StrongString<class date>;
using CpuFreq = StrongString<class cpufreq>;

using Block = std::variant<Date, CpuFreq>;
using Bar = std::vector<Block>;
