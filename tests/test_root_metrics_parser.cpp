#include <cassert>
#include <string>

#include "monitor/root_metrics_parser.hpp"

int main() {
  const std::string sample =
      "*** Running tasks ***\n"
      "Name              ID  CPU ms/s  %PCPU  Disk Read Bytes/s  Disk Write Bytes/s  Energy Impact\n"
      "clang            321      80.0   75.0            2048.0             1024.0            42\n"
      "python           777      20.0   10.0               0.0                0.0             3\n"
      "****\n";

  monitor::AmpData parsed = monitor::parse_amp_data(sample, false);
  assert(parsed.core_mix.at(321) == "P:75% E:25%");
  assert(parsed.io.at(321) == "2.0K/1.0K");
  assert(parsed.power.at(321) == "42");
  assert(parsed.core_mix.at(777) == "P:10% E:90%");
  assert(parsed.io.at(777) == "0B/0B");
  assert(parsed.power.at(777) == "3");

  const std::string super_sample =
      "*** Running tasks ***\n"
      "Name              PID  CPU ms/s  %SCPU  Energy Impact\n"
      "metal-runner      999      50.0   60.0            18\n"
      "****\n";
  parsed = monitor::parse_amp_data(super_sample, true);
  assert(parsed.core_mix.at(999) == "S:60% P:40%");
  assert(parsed.power.at(999) == "18");

  const std::string merged_spaces_sample =
      "*** Running tasks ***\n"
      "Name              ID  CPU ms/s  PCPU ms/s  Disk Read Bytes/s  Disk Write Bytes/s  Energy Impact\n"
      "swift-build       222      100.0 75.0              4096.0 2048.0             9\n"
      "****\n";
  parsed = monitor::parse_amp_data(merged_spaces_sample, false);
  assert(parsed.core_mix.at(222) == "P:75% E:25%");
  assert(parsed.io.at(222) == "4.0K/2.0K");
  assert(parsed.power.at(222) == "9");

  const std::string missing_columns_sample =
      "*** Running tasks ***\n"
      "Name              PID  CPU ms/s\n"
      "helper            333      20.0\n"
      "****\n";
  parsed = monitor::parse_amp_data(missing_columns_sample, false);
  assert(parsed.core_mix.at(333) == "P:0% E:100%");
  assert(parsed.io.find(333) == parsed.io.end());
  assert(parsed.power.find(333) == parsed.power.end());
  return 0;
}
