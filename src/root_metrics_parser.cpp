#include "monitor/root_metrics_parser.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace monitor {
namespace {

std::string trim_copy(const std::string& text) {
  const std::size_t start = text.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) {
    return "";
  }
  const std::size_t end = text.find_last_not_of(" \t\r\n");
  return text.substr(start, end - start + 1);
}

std::vector<std::string> split_multi_space(const std::string& line) {
  std::vector<std::string> result;
  std::string current;
  int space_run = 0;
  for (char ch : line) {
    if (ch == ' ') {
      ++space_run;
      if (space_run >= 2) {
        if (!current.empty()) {
          result.push_back(trim_copy(current));
          current.clear();
        }
      } else if (!current.empty()) {
        current.push_back(ch);
      }
    } else {
      space_run = 0;
      current.push_back(ch);
    }
  }
  if (!current.empty()) {
    result.push_back(trim_copy(current));
  }
  return result;
}

std::string lowercase_copy(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return text;
}

std::string compact_io_bytes(double value) {
  static const char* units[] = {"B", "K", "M", "G", "T"};
  int unit = 0;
  while (std::fabs(value) >= 1024.0 && unit < 4) {
    value /= 1024.0;
    ++unit;
  }
  char buffer[16];
  if (unit == 0 || std::fabs(value) >= 10.0) {
    std::snprintf(buffer, sizeof(buffer), "%.0f%s", value, units[unit]);
  } else {
    std::snprintf(buffer, sizeof(buffer), "%.1f%s", value, units[unit]);
  }
  return buffer;
}

bool is_numeric_field(const std::string& text) {
  const std::string trimmed = trim_copy(text);
  if (trimmed.empty()) {
    return false;
  }
  bool has_digit = false;
  for (char ch : trimmed) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    if (std::isdigit(uch)) {
      has_digit = true;
      continue;
    }
    if (ch == '.' || ch == '+' || ch == '-' || ch == 'e' || ch == 'E') {
      continue;
    }
    return false;
  }
  return has_digit;
}

std::vector<std::string> split_numeric_subfields(const std::string& field) {
  std::vector<std::string> parts;
  std::istringstream stream(field);
  std::string part;
  while (stream >> part) {
    if (!is_numeric_field(part)) {
      return {};
    }
    parts.push_back(part);
  }
  if (parts.size() < 2) {
    return {};
  }
  return parts;
}

void split_merged_numeric_fields(std::vector<std::string>& fields, std::size_t expected_size) {
  for (std::string& field : fields) {
    field = trim_copy(field);
  }
  while (fields.size() < expected_size) {
    bool changed = false;
    for (std::size_t index = 0; index < fields.size(); ++index) {
      const std::string& field = fields[index];
      std::vector<std::string> parts = split_numeric_subfields(field);
      if (parts.empty()) {
        continue;
      }
      fields[index] = std::move(parts.front());
      fields.insert(fields.begin() + static_cast<std::ptrdiff_t>(index + 1), parts.begin() + 1, parts.end());
      changed = true;
      break;
    }
    if (!changed) {
      break;
    }
  }
}

std::vector<std::string> expand_amp_header(const std::vector<std::string>& fields) {
  std::vector<std::string> expanded;
  for (const std::string& field : fields) {
    if (field.rfind("Deadlines (", 0) == 0) {
      expanded.push_back("Deadline<2ms");
      expanded.push_back("Deadline2-5ms");
    } else if (field.rfind("Wakeups (", 0) == 0) {
      expanded.push_back("WakeupsIntr");
      expanded.push_back("WakeupsPkgIdle");
    } else {
      const std::size_t pos = field.find(" %");
      if (pos != std::string::npos) {
        expanded.push_back(field.substr(0, pos));
        expanded.push_back("%" + field.substr(pos + 2));
      } else {
        expanded.push_back(field);
      }
    }
  }
  return expanded;
}

}  // namespace

AmpData parse_amp_data(const std::string& text, bool has_super) {
  AmpData result;
  std::istringstream input(text);
  std::string line;
  bool in_tasks = false;
  std::vector<std::string> header;
  while (std::getline(input, line)) {
    const std::string trimmed = trim_copy(line);
    if (trimmed.find("*** Running tasks ***") != std::string::npos) {
      in_tasks = true;
      continue;
    }
    if (!in_tasks) {
      continue;
    }
    if (trimmed.rfind("****", 0) == 0) {
      break;
    }
    if (trimmed.empty()) {
      continue;
    }
    if (trimmed.rfind("Name", 0) == 0 && trimmed.find("CPU ms/s") != std::string::npos) {
      header = expand_amp_header(split_multi_space(trimmed));
      continue;
    }
    if (header.empty()) {
      continue;
    }
    std::vector<std::string> values = split_multi_space(trimmed);
    split_merged_numeric_fields(values, header.size());
    if (values.size() != header.size()) {
      continue;
    }
    std::map<std::string, std::string> row;
    for (std::size_t i = 0; i < header.size(); ++i) {
      row[trim_copy(header[i])] = trim_copy(values[i]);
    }
    const auto id_it = row.find("ID") != row.end() ? row.find("ID") : row.find("PID");
    if (id_it == row.end()) {
      continue;
    }
    const int pid = std::atoi(id_it->second.c_str());
    if (pid <= 0) {
      continue;
    }

    double primary_percent = 0.0;
    const char* primary_label = nullptr;
    const char* secondary_label = nullptr;

    if (row.count("%SCPU")) {
      primary_percent = std::atof(row["%SCPU"].c_str());
      primary_label = has_super ? "S" : "P";
      secondary_label = has_super ? "P" : "E";
    } else if (row.count("SCPU ms/s") && row.count("CPU ms/s")) {
      const double total = std::atof(row["CPU ms/s"].c_str());
      const double primary_ms = std::atof(row["SCPU ms/s"].c_str());
      if (total > 0.0) {
        primary_percent = primary_ms * 100.0 / total;
      }
      primary_label = has_super ? "S" : "P";
      secondary_label = has_super ? "P" : "E";
    } else if (row.count("%PCPU")) {
      primary_percent = std::atof(row["%PCPU"].c_str());
      primary_label = "P";
      secondary_label = "E";
    } else if (row.count("PCPU ms/s") && row.count("CPU ms/s")) {
      const double total = std::atof(row["CPU ms/s"].c_str());
      const double primary_ms = std::atof(row["PCPU ms/s"].c_str());
      if (total > 0.0) {
        primary_percent = primary_ms * 100.0 / total;
      }
      primary_label = "P";
      secondary_label = "E";
    }

    if (!primary_label) {
      primary_label = "P";
      secondary_label = "E";
    }

    const double secondary_percent = std::max(0.0, 100.0 - primary_percent);
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%s:%d%% %s:%d%%",
                  primary_label,
                  static_cast<int>(std::round(primary_percent)),
                  secondary_label,
                  static_cast<int>(std::round(secondary_percent)));
    result.core_mix[pid] = buffer;

    double disk_read = 0.0;
    double disk_write = 0.0;
    bool saw_io_column = false;
    for (const auto& [col, val] : row) {
      const std::string lower_col = lowercase_copy(col);
      const bool io_column = lower_col.find("disk") != std::string::npos ||
                             lower_col.find("io") != std::string::npos ||
                             lower_col.find("bytes") != std::string::npos;
      if (!io_column) {
        continue;
      }
      saw_io_column = true;
      if (lower_col.find("read") != std::string::npos) {
        disk_read += std::atof(val.c_str());
      } else if (lower_col.find("write") != std::string::npos ||
                 lower_col.find("written") != std::string::npos) {
        disk_write += std::atof(val.c_str());
      }
    }
    if (saw_io_column) {
      char buf[32];
      std::snprintf(buf, sizeof(buf), "%s/%s", compact_io_bytes(disk_read).c_str(), compact_io_bytes(disk_write).c_str());
      result.io[pid] = buf;
    }

    for (const auto& [col, val] : row) {
      const std::string lower_col = lowercase_copy(col);
      if (lower_col.find("energy") != std::string::npos || lower_col.find("impact") != std::string::npos) {
        const double energy = std::atof(val.c_str());
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%.0f", energy);
        result.power[pid] = buf;
        break;
      }
    }
  }
  return result;
}

}  // namespace monitor
