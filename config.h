#include <string>
#include <vector>
#include <variant>

#define REFRESHRATE 1000


template <class Tag> struct StrongString {
  std::string stat;
};

using Date = StrongString<class date>;
using CpuFreq = StrongString<class cpufreq>;
using Battery = StrongString<class battery>;

using Block = std::variant<Date, CpuFreq, Battery>;
using Bar = std::vector<Block>;
