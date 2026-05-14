#pragma once

#include <optional>
#include <string_view>

namespace monitor {

enum class ViewProfile {
  Alpha,
  Beta,
};

const char* view_profile_name(ViewProfile profile);
const char* view_profile_label(ViewProfile profile);
std::optional<ViewProfile> parse_view_profile(std::string_view value);

}  // namespace monitor
