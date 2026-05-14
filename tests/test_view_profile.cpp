#include <cassert>
#include <string>

#include "monitor/ui/view_profile.hpp"

int main() {
  assert(monitor::view_profile_name(monitor::ViewProfile::Alpha) == std::string("alpha"));
  assert(monitor::view_profile_name(monitor::ViewProfile::Beta) == std::string("beta"));
  assert(monitor::view_profile_label(monitor::ViewProfile::Alpha) == std::string("Alpha"));
  assert(monitor::view_profile_label(monitor::ViewProfile::Beta) == std::string("Beta"));

  auto profile = monitor::parse_view_profile("alpha");
  assert(profile.has_value());
  assert(*profile == monitor::ViewProfile::Alpha);

  profile = monitor::parse_view_profile(" Beta ");
  assert(profile.has_value());
  assert(*profile == monitor::ViewProfile::Beta);

  assert(!monitor::parse_view_profile("classic").has_value());
  assert(!monitor::parse_view_profile("").has_value());
  return 0;
}
