#include "monitor/ui/view_profile.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace monitor {
namespace {

std::string trim_lower(std::string_view value) {
  const std::size_t start = value.find_first_not_of(" \t\r\n");
  if (start == std::string_view::npos) {
    return "";
  }
  const std::size_t end = value.find_last_not_of(" \t\r\n");
  std::string result(value.substr(start, end - start + 1));
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return result;
}

}  // namespace

const char* view_profile_name(ViewProfile profile) {
  switch (profile) {
    case ViewProfile::Alpha: return "alpha";
    case ViewProfile::Beta: return "beta";
  }
  return "alpha";
}

const char* view_profile_label(ViewProfile profile) {
  switch (profile) {
    case ViewProfile::Alpha: return "Alpha";
    case ViewProfile::Beta: return "Beta";
  }
  return "Alpha";
}

std::optional<ViewProfile> parse_view_profile(std::string_view value) {
  const std::string lowered = trim_lower(value);
  if (lowered == "alpha") {
    return ViewProfile::Alpha;
  }
  if (lowered == "beta") {
    return ViewProfile::Beta;
  }
  return std::nullopt;
}

}  // namespace monitor
