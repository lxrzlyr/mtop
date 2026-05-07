#pragma once

#include <map>
#include <string>

namespace monitor {

struct AmpData {
  std::map<int, std::string> core_mix;
  std::map<int, std::string> io;
  std::map<int, std::string> power;
};

AmpData parse_amp_data(const std::string& text, bool has_super);

}  // namespace monitor
