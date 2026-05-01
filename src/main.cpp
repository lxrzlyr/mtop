#include <algorithm>
#include <chrono>
#include <cmath>
#include <clocale>
#include <csignal>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <curses.h>
#include <sys/resource.h>

#include "monitor/config.hpp"
#include "monitor/sampler.hpp"

namespace {

enum class SortMode { Pid, Cpu, Mem, Time, Name };
enum class PromptMode { None, Search, Filter, Sort, Signal };
enum class SetupField { Theme, CachedMemory, Refresh, Done };
constexpr int kFunctionKeyCount = 10;

struct ButtonBounds {
  int x0 = -1;
  int x1 = -1;
  int event = ERR;
};

struct UiState {
  SortMode sort = SortMode::Cpu;
  bool tree_mode = false;
  int page = 0;
  std::optional<int> selected_pid;
  PromptMode prompt_mode = PromptMode::None;
  std::string filter;
  std::string search;
  std::string prompt_buffer;
  std::string prompt_saved;
  std::string pid_search;
  std::chrono::steady_clock::time_point pid_search_deadline{};
  std::string status_message;
  std::chrono::steady_clock::time_point status_deadline{};
  bool search_pending = false;
  int sort_direction = -1;
  bool show_help = false;
  bool show_setup = false;
  bool show_cached_memory = false;
  SetupField setup_field = SetupField::Theme;
  ButtonBounds footer_buttons[kFunctionKeyCount];
  std::set<int> collapsed_pids;
};

struct RuntimeOptions {
  bool demo = false;
  bool help = false;
  bool version = false;
  std::string config_path;
  std::optional<int> refresh_ms;
  std::optional<std::string> theme;
  std::optional<int> process_limit;
};

struct WindowLayout {
  WINDOW* gpu = nullptr;
  WINDOW* cpu = nullptr;
  WINDOW* memory = nullptr;
  WINDOW* processes = nullptr;
  WINDOW* footer = nullptr;
  int rows = 0;
  int cols = 0;
};

struct ProcessRow {
  const monitor::ProcessSnapshot* process = nullptr;
  int depth = 0;
  bool has_children = false;
  bool collapsed = false;
};

struct ProcessViewMetrics {
  int page_rows = 1;
  int row_count = 0;
  int page_count = 1;
  int selected_index = -1;
};

struct ProcessColumn {
  const char* title;
  int width;
  SortMode sort_mode;
  bool sortable;
};

constexpr ProcessColumn kProcessColumns[] = {
    {"PID", 5, SortMode::Pid, true},
    {"USER", 10, SortMode::Name, false},
    {"PRI", 4, SortMode::Pid, false},
    {"NI", 3, SortMode::Pid, false},
    {"VIRT", 6, SortMode::Mem, false},
    {"RES", 6, SortMode::Mem, false},
    {"S", 1, SortMode::Pid, false},
    {"CPU%", 5, SortMode::Cpu, true},
    {"MEM%", 5, SortMode::Mem, true},
    {"TIME+", 8, SortMode::Time, true},
};

RuntimeOptions parse_args(int argc, char** argv) {
  RuntimeOptions options;
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg(argv[i]);
    if (arg == "--demo") {
      options.demo = true;
    } else if (arg == "--help" || arg == "-h") {
      options.help = true;
    } else if (arg == "--version" || arg == "-v") {
      options.version = true;
    } else if (arg == "--config" && i + 1 < argc) {
      options.config_path = argv[++i];
    } else if (arg == "--refresh-ms" && i + 1 < argc) {
      options.refresh_ms = std::atoi(argv[++i]);
    } else if (arg == "--process-limit" && i + 1 < argc) {
      options.process_limit = std::atoi(argv[++i]);
    } else if (arg == "--theme" && i + 1 < argc) {
      options.theme = std::string(argv[++i]);
    }
  }
  return options;
}

std::string human_bytes(std::uint64_t value) {
  static const char* units[] = {"B", "KB", "MB", "GB", "TB"};
  double current = static_cast<double>(value);
  int unit = 0;
  while (current >= 1024.0 && unit < 4) {
    current /= 1024.0;
    ++unit;
  }
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%.1f%s", current, units[unit]);
  return buffer;
}

std::string compact_bytes(std::uint64_t value) {
  static const char* units[] = {"B", "K", "M", "G", "T"};
  double current = static_cast<double>(value);
  int unit = 0;
  while (current >= 1024.0 && unit < 4) {
    current /= 1024.0;
    ++unit;
  }
  char buffer[32];
  if (unit == 0) {
    std::snprintf(buffer, sizeof(buffer), "%llu%s",
                  static_cast<unsigned long long>(value),
                  units[unit]);
  } else {
    std::snprintf(buffer, sizeof(buffer), "%.0f%s", current, units[unit]);
  }
  return buffer;
}

std::string format_uptime(std::uint64_t seconds) {
  const std::uint64_t days = seconds / 86400;
  seconds %= 86400;
  const std::uint64_t hours = seconds / 3600;
  seconds %= 3600;
  const std::uint64_t minutes = seconds / 60;
  char buffer[64];
  if (days > 0) {
    std::snprintf(buffer, sizeof(buffer), "%llud %02llu:%02llu",
                  static_cast<unsigned long long>(days),
                  static_cast<unsigned long long>(hours),
                  static_cast<unsigned long long>(minutes));
  } else {
    std::snprintf(buffer, sizeof(buffer), "%02llu:%02llu",
                  static_cast<unsigned long long>(hours),
                  static_cast<unsigned long long>(minutes));
  }
  return buffer;
}

std::string format_cpu_time(std::uint64_t total_ns) {
  const std::uint64_t total_cs = total_ns / 10000000ULL;
  const std::uint64_t total_seconds = total_cs / 100;
  const std::uint64_t hundredths = total_cs % 100;
  const std::uint64_t minutes = total_seconds / 60;
  const std::uint64_t seconds = total_seconds % 60;

  char buffer[32];
  if (minutes < 60) {
    std::snprintf(buffer, sizeof(buffer), "%llu:%02llu.%02llu",
                  static_cast<unsigned long long>(minutes),
                  static_cast<unsigned long long>(seconds),
                  static_cast<unsigned long long>(hundredths));
  } else {
    const std::uint64_t hours = minutes / 60;
    const std::uint64_t rem_minutes = minutes % 60;
    std::snprintf(buffer, sizeof(buffer), "%llu:%02llu:%02llu",
                  static_cast<unsigned long long>(hours),
                  static_cast<unsigned long long>(rem_minutes),
                  static_cast<unsigned long long>(seconds));
  }
  return buffer;
}

std::string format_gib(std::uint64_t bytes) {
  const double gib = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%.3fGi", gib);
  return buffer;
}

std::string elide_right(const std::string& text, int width) {
  if (width <= 0) {
    return "";
  }
  if (static_cast<int>(text.size()) <= width) {
    return text;
  }
  if (width <= 3) {
    return text.substr(0, width);
  }
  return text.substr(0, width - 3) + "...";
}

std::string lowercase(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return text;
}

int process_column_offset(int column_index) {
  int x = 1;
  for (int i = 0; i < column_index; ++i) {
    x += kProcessColumns[i].width + 1;
  }
  return x;
}

int process_command_offset() {
  return process_column_offset(static_cast<int>(std::size(kProcessColumns)));
}

std::string centered_text(const std::string& text, int width) {
  const std::string clipped = elide_right(text, width);
  if (static_cast<int>(clipped.size()) >= width) {
    return clipped;
  }
  const int left = (width - static_cast<int>(clipped.size())) / 2;
  const int right = width - static_cast<int>(clipped.size()) - left;
  return std::string(left, ' ') + clipped + std::string(right, ' ');
}

std::optional<SortMode> process_sort_from_header_x(int local_x) {
  for (int column = 0; column < static_cast<int>(std::size(kProcessColumns)); ++column) {
    const ProcessColumn& def = kProcessColumns[column];
    const int x0 = process_column_offset(column);
    const int x1 = x0 + def.width - 1;
    if (local_x >= x0 && local_x <= x1 && def.sortable) {
      return def.sort_mode;
    }
  }
  return std::nullopt;
}

struct MeterSegment {
  double percent = 0.0;
  int color_pair = 1;
  int attr = A_NORMAL;
};

void draw_stacked_meter(WINDOW* window, int y, int x, int width, const std::vector<MeterSegment>& segments) {
  static const char* blocks[] = {" ", "▏", "▎", "▍", "▌", "▋", "▊", "▉", "█"};
  std::vector<int> ownership(width, -1);
  std::vector<int> cell_units(width, 0);
  int cursor_units = 0;
  for (int segment_index = 0; segment_index < static_cast<int>(segments.size()); ++segment_index) {
    const int segment_units = std::max(0, static_cast<int>(std::round(segments[segment_index].percent / 100.0 * width * 8.0)));
    for (int unit = 0; unit < segment_units && cursor_units < width * 8; ++unit, ++cursor_units) {
      const int cell = cursor_units / 8;
      ownership[cell] = segment_index;
      cell_units[cell] = std::min(8, cursor_units % 8 + 1);
    }
  }

  for (int i = 0; i < width; ++i) {
    if (ownership[i] >= 0) {
      const MeterSegment& segment = segments[ownership[i]];
      wattron(window, segment.attr);
      if (has_colors()) wattron(window, COLOR_PAIR(segment.color_pair));
      mvwaddstr(window, y, x + i, blocks[cell_units[i]]);
      if (has_colors()) wattroff(window, COLOR_PAIR(segment.color_pair));
      wattroff(window, segment.attr);
    } else {
      mvwaddch(window, y, x + i, ' ');
    }
  }
}

void draw_meter(WINDOW* window, int y, int x, int width, double percent, int fill_color_pair) {
  static const char* blocks[] = {" ", "▏", "▎", "▍", "▌", "▋", "▊", "▉", "█"};
  const int total_units = std::clamp(static_cast<int>(std::round(percent / 100.0 * width * 8.0)), 0, width * 8);
  for (int i = 0; i < width; ++i) {
    const int cell_units = std::clamp(total_units - i * 8, 0, 8);
    if (cell_units > 0) {
      if (has_colors()) wattron(window, COLOR_PAIR(fill_color_pair));
      mvwaddstr(window, y, x + i, blocks[cell_units]);
      if (has_colors()) wattroff(window, COLOR_PAIR(fill_color_pair));
    } else {
      mvwaddch(window, y, x + i, ' ');
    }
  }
}

void apply_theme(const monitor::AppConfig& config) {
  if (!has_colors()) {
    return;
  }
  start_color();
  use_default_colors();
  if (config.theme == "mono") {
    init_pair(1, COLOR_WHITE, -1);
    init_pair(2, COLOR_WHITE, -1);
    init_pair(3, COLOR_WHITE, -1);
    init_pair(4, COLOR_BLACK, COLOR_WHITE);
    init_pair(5, COLOR_BLACK, COLOR_WHITE);
    init_pair(6, COLOR_WHITE, -1);
    init_pair(7, COLOR_WHITE, -1);
    init_pair(8, COLOR_WHITE, -1);
    init_pair(9, COLOR_WHITE, -1);
    init_pair(10, COLOR_WHITE, -1);
    init_pair(11, COLOR_WHITE, -1);
    init_pair(12, COLOR_WHITE, -1);
    init_pair(13, COLOR_WHITE, -1);
    init_pair(14, COLOR_WHITE, -1);
    init_pair(15, COLOR_BLACK, COLOR_WHITE);
  } else if (config.theme == "green") {
    init_pair(1, COLOR_WHITE, -1);
    init_pair(2, COLOR_CYAN, -1);
    init_pair(3, COLOR_RED, -1);
    init_pair(4, COLOR_BLACK, COLOR_GREEN);
    init_pair(5, COLOR_BLACK, COLOR_CYAN);
    init_pair(6, COLOR_CYAN, -1);
    init_pair(7, COLOR_GREEN, -1);
    init_pair(8, COLOR_MAGENTA, -1);
    init_pair(9, COLOR_WHITE, -1);
    init_pair(10, COLOR_BLUE, -1);
    init_pair(11, COLOR_YELLOW, -1);
    init_pair(12, COLOR_CYAN, -1);
    init_pair(13, COLOR_CYAN, -1);
    init_pair(14, COLOR_YELLOW, -1);
    init_pair(15, COLOR_BLACK, COLOR_CYAN);
  } else {
    init_pair(1, COLOR_WHITE, -1);
    init_pair(2, COLOR_CYAN, -1);
    init_pair(3, COLOR_RED, -1);
    init_pair(4, COLOR_BLACK, COLOR_GREEN);
    init_pair(5, COLOR_BLACK, COLOR_CYAN);
    init_pair(6, COLOR_CYAN, -1);
    init_pair(7, COLOR_GREEN, -1);
    init_pair(8, COLOR_MAGENTA, -1);
    init_pair(9, COLOR_WHITE, -1);
    init_pair(10, COLOR_BLUE, -1);
    init_pair(11, COLOR_YELLOW, -1);
    init_pair(12, COLOR_CYAN, -1);
    init_pair(13, COLOR_CYAN, -1);
    init_pair(14, COLOR_YELLOW, -1);
    init_pair(15, COLOR_BLACK, COLOR_CYAN);
  }
}

void print_help() {
  std::printf(
      "mtop - Apple Silicon terminal monitor\n\n"
      "Usage:\n"
      "  mtop [--demo] [--theme THEME] [--refresh-ms MS] [--process-limit N] [--config PATH]\n\n"
      "Options:\n"
      "  --demo              Run with synthetic data for UI preview\n"
      "  --theme THEME       Theme name: apple, green, mono\n"
      "  --refresh-ms MS     Refresh interval in milliseconds\n"
      "  --process-limit N   Number of visible process rows\n"
      "  --config PATH       Explicit config file path\n"
      "  --help, -h          Show this help\n"
      "  --version, -v       Show version\n\n"
      "Default config path:\n"
      "  %s\n",
      monitor::config_default_path().c_str());
}

void print_version() {
  std::printf("mtop 1.0.0\n");
}

std::string sort_mode_name(SortMode mode) {
  switch (mode) {
    case SortMode::Pid: return "pid";
    case SortMode::Cpu: return "cpu";
    case SortMode::Mem: return "mem";
    case SortMode::Time: return "time";
    case SortMode::Name: return "name";
  }
  return "cpu";
}

SortMode cycle_sort(SortMode mode) {
  switch (mode) {
    case SortMode::Pid: return SortMode::Cpu;
    case SortMode::Cpu: return SortMode::Mem;
    case SortMode::Mem: return SortMode::Time;
    case SortMode::Time: return SortMode::Name;
    case SortMode::Name: return SortMode::Pid;
  }
  return SortMode::Cpu;
}

int default_sort_direction(SortMode mode) {
  switch (mode) {
    case SortMode::Pid: return 1;
    case SortMode::Name: return 1;
    case SortMode::Cpu:
    case SortMode::Mem:
    case SortMode::Time:
      return -1;
  }
  return -1;
}

std::string cpu_topology_summary(const monitor::SystemSnapshot& snapshot) {
  int super_count = 0;
  int performance_count = 0;
  int efficiency_count = 0;
  for (const auto& core : snapshot.cpu_cores) {
    if (core.cluster_type == "Super") {
      ++super_count;
    } else if (core.cluster_type == "Performance") {
      ++performance_count;
    } else if (core.cluster_type == "Efficiency") {
      ++efficiency_count;
    }
  }

  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "S%d+P%d+E%d",
                super_count,
                performance_count,
                efficiency_count);
  return buffer;
}

double gpu_memory_percent(const monitor::SystemSnapshot& snapshot) {
  if (snapshot.gpu_memory_total_bytes == 0) {
    return 0.0;
  }
  return static_cast<double>(snapshot.gpu_memory_used_bytes) * 100.0 /
         static_cast<double>(snapshot.gpu_memory_total_bytes);
}

std::string bar_for_percent(double percent, int width) {
  const int filled = std::clamp(static_cast<int>(std::round(percent / 100.0 * width)), 0, width);
  return std::string(filled, '|') + std::string(width - filled, ' ');
}

void draw_info_bar(const monitor::SystemSnapshot& snapshot, int cols) {
  move(0, 0);
  clrtoeol();
  std::string line = snapshot.soc_name +
                     " | CPU " + std::to_string(snapshot.cpu_core_count) +
                     " (" + cpu_topology_summary(snapshot) + ")" +
                     " | GPU " + std::to_string(snapshot.gpu_core_count) +
                     " | Uptime: " + format_uptime(snapshot.uptime_seconds) +
                     " | Battery: " + snapshot.battery.description +
                     " | ANE: " + snapshot.ane;
  if (static_cast<int>(line.size()) > cols - 1) {
    line.resize(std::max(0, cols - 1));
  }
  mvprintw(0, 0, "%s", line.c_str());
}

bool process_matches_query(const monitor::ProcessSnapshot& process, const std::string& query) {
  if (query.empty()) {
    return true;
  }
  const std::string needle = lowercase(query);
  return lowercase(process.user).find(needle) != std::string::npos ||
         lowercase(process.name).find(needle) != std::string::npos ||
         lowercase(process.command).find(needle) != std::string::npos;
}

bool process_sort_less(const monitor::ProcessSnapshot* lhs,
                       const monitor::ProcessSnapshot* rhs,
                       SortMode mode,
                       int direction) {
  switch (mode) {
    case SortMode::Pid:
      if (lhs->pid != rhs->pid) return direction > 0 ? lhs->pid < rhs->pid : lhs->pid > rhs->pid;
      break;
    case SortMode::Cpu:
      if (lhs->cpu_percent != rhs->cpu_percent) return direction > 0 ? lhs->cpu_percent < rhs->cpu_percent : lhs->cpu_percent > rhs->cpu_percent;
      break;
    case SortMode::Mem:
      if (lhs->memory_percent != rhs->memory_percent) return direction > 0 ? lhs->memory_percent < rhs->memory_percent : lhs->memory_percent > rhs->memory_percent;
      break;
    case SortMode::Time:
      if (lhs->total_cpu_time_ns != rhs->total_cpu_time_ns) return direction > 0 ? lhs->total_cpu_time_ns < rhs->total_cpu_time_ns : lhs->total_cpu_time_ns > rhs->total_cpu_time_ns;
      break;
    case SortMode::Name:
      if (lhs->command != rhs->command) return direction > 0 ? lhs->command < rhs->command : lhs->command > rhs->command;
      break;
  }
  return lhs->pid < rhs->pid;
}

std::vector<ProcessRow> build_process_rows(const monitor::SystemSnapshot& snapshot, const UiState& state) {
  std::vector<const monitor::ProcessSnapshot*> filtered;
  filtered.reserve(snapshot.processes.size());
  for (const auto& process : snapshot.processes) {
    if (process_matches_query(process, state.filter)) {
      filtered.push_back(&process);
    }
  }

  auto less = [&](const monitor::ProcessSnapshot* lhs, const monitor::ProcessSnapshot* rhs) {
    return process_sort_less(lhs, rhs, state.sort, state.sort_direction);
  };

  if (!state.tree_mode) {
    std::sort(filtered.begin(), filtered.end(), less);
    std::vector<ProcessRow> rows;
    rows.reserve(filtered.size());
    for (const auto* process : filtered) {
      rows.push_back({process, 0, false, false});
    }
    return rows;
  }

  std::set<int> pid_set;
  std::map<int, std::vector<const monitor::ProcessSnapshot*>> children;
  for (const auto* process : filtered) {
    pid_set.insert(process->pid);
    children[process->parent_pid].push_back(process);
  }
  for (auto& entry : children) {
    std::sort(entry.second.begin(), entry.second.end(), less);
  }

  std::vector<const monitor::ProcessSnapshot*> roots;
  roots.reserve(filtered.size());
  for (const auto* process : filtered) {
    if (process->parent_pid <= 0 || process->parent_pid == process->pid ||
        pid_set.find(process->parent_pid) == pid_set.end()) {
      roots.push_back(process);
    }
  }
  std::sort(roots.begin(), roots.end(), less);

  std::vector<ProcessRow> rows;
  std::set<int> visited;
  std::function<void(const monitor::ProcessSnapshot*, int)> dfs =
      [&](const monitor::ProcessSnapshot* process, int depth) {
        if (!process || visited.find(process->pid) != visited.end()) {
          return;
        }
        visited.insert(process->pid);
        const auto child_it = children.find(process->pid);
        const bool has_children = child_it != children.end() && !child_it->second.empty();
        const bool collapsed = has_children && state.collapsed_pids.find(process->pid) != state.collapsed_pids.end();
        rows.push_back({process, depth, has_children, collapsed});
        if (collapsed) {
          return;
        }
        if (child_it == children.end()) {
          return;
        }
        for (const auto* child : child_it->second) {
          dfs(child, depth + 1);
        }
      };

  for (const auto* root : roots) {
    dfs(root, 0);
  }
  for (const auto* process : filtered) {
    dfs(process, 0);
  }
  return rows;
}

int selected_process_index(const std::vector<ProcessRow>& rows, const UiState& state) {
  if (!state.selected_pid.has_value()) {
    return -1;
  }
  for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
    if (rows[i].process->pid == *state.selected_pid) {
      return i;
    }
  }
  return -1;
}

void set_status(UiState& state, const std::string& message, int duration_ms = 3000) {
  state.status_message = message;
  state.status_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(duration_ms);
}

void set_sort_mode(UiState& state, SortMode mode, bool keep_direction = false) {
  state.sort = mode;
  if (!keep_direction) {
    state.sort_direction = default_sort_direction(mode);
  }
  state.page = 0;
  state.selected_pid.reset();
}

void apply_search_request(UiState& state, const std::vector<ProcessRow>& rows) {
  if (!state.search_pending) {
    return;
  }
  state.search_pending = false;
  if (state.search.empty() || rows.empty()) {
    return;
  }

  const int current_index = selected_process_index(rows, state);
  const int base_index = current_index >= 0 ? current_index : -1;
  for (int offset = 1; offset <= static_cast<int>(rows.size()); ++offset) {
    const int index = (base_index + offset + static_cast<int>(rows.size())) % static_cast<int>(rows.size());
    if (process_matches_query(*rows[index].process, state.search)) {
      state.selected_pid = rows[index].process->pid;
      return;
    }
  }
  set_status(state, "No search match: " + state.search);
}

ProcessViewMetrics calculate_process_metrics(const std::vector<ProcessRow>& rows,
                                             UiState& state,
                                             int window_height) {
  ProcessViewMetrics metrics;
  metrics.page_rows = std::max(1, window_height - 3);
  metrics.row_count = static_cast<int>(rows.size());
  if (rows.empty()) {
    state.page = 0;
    state.selected_pid.reset();
    return metrics;
  }

  metrics.selected_index = selected_process_index(rows, state);
  metrics.page_count = std::max(1, (metrics.row_count + metrics.page_rows - 1) / metrics.page_rows);
  state.page = std::clamp(state.page, 0, metrics.page_count - 1);
  if (metrics.selected_index >= 0 &&
      (metrics.selected_index < state.page * metrics.page_rows ||
       metrics.selected_index >= (state.page + 1) * metrics.page_rows)) {
    state.page = metrics.selected_index / metrics.page_rows;
  }
  return metrics;
}

void move_selection(UiState& state, const std::vector<ProcessRow>& rows, int delta) {
  if (rows.empty()) {
    state.selected_pid.reset();
    return;
  }
  int index = selected_process_index(rows, state);
  if (index < 0) {
    index = delta >= 0 ? 0 : static_cast<int>(rows.size()) - 1;
    state.selected_pid = rows[index].process->pid;
    return;
  }
  index = std::clamp(index + delta, 0, static_cast<int>(rows.size()) - 1);
  state.selected_pid = rows[index].process->pid;
}

void apply_pid_search(UiState& state, const std::vector<ProcessRow>& rows, int ch) {
  const auto now = std::chrono::steady_clock::now();
  if (now > state.pid_search_deadline) {
    state.pid_search.clear();
  }
  state.pid_search.push_back(static_cast<char>(ch));
  state.pid_search_deadline = now + std::chrono::milliseconds(1500);

  for (const auto& row : rows) {
    const std::string pid_text = std::to_string(row.process->pid);
    if (pid_text.rfind(state.pid_search, 0) == 0) {
      state.selected_pid = row.process->pid;
      return;
    }
  }
  set_status(state, "No PID match: " + state.pid_search, 1200);
}

void expand_selected(UiState& state) {
  if (state.selected_pid.has_value()) {
    state.collapsed_pids.erase(*state.selected_pid);
  }
}

void collapse_selected(UiState& state, const std::vector<ProcessRow>& rows) {
  if (!state.selected_pid.has_value()) {
    return;
  }
  for (const auto& row : rows) {
    if (row.process->pid == *state.selected_pid && row.has_children) {
      state.collapsed_pids.insert(*state.selected_pid);
      return;
    }
  }
}

void toggle_all_tree_nodes(UiState& state, const std::vector<ProcessRow>& rows) {
  bool has_any_collapsed = !state.collapsed_pids.empty();
  state.collapsed_pids.clear();
  if (!has_any_collapsed) {
    for (const auto& row : rows) {
      if (row.has_children) {
        state.collapsed_pids.insert(row.process->pid);
      }
    }
  }
}

std::vector<double> resample_history(const std::vector<double>& history, int sample_count, int max_visible_raw_samples) {
  std::vector<double> samples(sample_count, 0.0);
  if (sample_count <= 0 || max_visible_raw_samples <= 0 || history.empty()) {
    return samples;
  }

  const int history_size = static_cast<int>(history.size());
  const int visible_raw_samples = std::min(max_visible_raw_samples, history_size);
  const int start = history_size - visible_raw_samples;
  for (int bucket = 0; bucket < sample_count; ++bucket) {
    const int slice_start = start + (bucket * visible_raw_samples) / sample_count;
    const int slice_end = start + ((bucket + 1) * visible_raw_samples) / sample_count;
    const int end = std::max(slice_start + 1, slice_end);
    double sum = 0.0;
    int count = 0;
    for (int index = slice_start; index < end && index < history_size; ++index) {
      sum += history[index];
      ++count;
    }
    if (count > 0) {
      samples[bucket] = sum / static_cast<double>(count);
    }
  }
  return samples;
}

static inline int nvtop_data_level(double rows, double data, double increment) {
  return static_cast<int>(rows - std::round(data / increment));
}

void nvtop_line_plot_port(WINDOW* win,
                          size_t num_data,
                          const double* data,
                          unsigned num_lines,
                          bool legend_left,
                          char legend[4][32]) {
  if (num_data == 0 || num_lines == 0) {
    return;
  }

  int rows = 0;
  int cols = 0;
  getmaxyx(win, rows, cols);
  rows -= 1;
  if (rows <= 0 || cols <= 0) {
    return;
  }
  const double increment = 100.0 / static_cast<double>(rows);
  static const short plot_line_colors[4] = {13, 14, 13, 14};

  unsigned lvl_before[4] = {0, 0, 0, 0};
  for (unsigned line = 0; line < num_lines; ++line) {
    lvl_before[line] = nvtop_data_level(rows, data[line], increment);
  }

  for (size_t offset = 0; offset < num_data && offset < static_cast<size_t>(cols); offset += num_lines) {
    for (unsigned line = 0; line < num_lines; ++line) {
      const size_t data_index = offset + line;
      if (data_index >= num_data || static_cast<int>(data_index) >= cols) {
        continue;
      }

      const unsigned lvl_now = nvtop_data_level(rows, data[data_index], increment);
      if (has_colors()) {
        wcolor_set(win, plot_line_colors[line], nullptr);
      }

      if (lvl_before[line] != lvl_now) {
        const bool drawing_down = lvl_before[line] < lvl_now;
        const unsigned bottom = drawing_down ? lvl_before[line] : lvl_now;
        const unsigned top = drawing_down ? lvl_now : lvl_before[line];

        mvwaddch(win, bottom, static_cast<int>(data_index), drawing_down ? ACS_URCORNER : ACS_ULCORNER);
        mvwaddch(win, top, static_cast<int>(data_index), drawing_down ? ACS_LLCORNER : ACS_LRCORNER);
        if (top - bottom > 1) {
          mvwvline(win, bottom + 1, static_cast<int>(data_index), ACS_VLINE, top - bottom - 1);
        }

        for (unsigned other = 0; other < num_lines; ++other) {
          if (other == line) {
            continue;
          }
          if (lvl_before[other] == top) {
            mvwaddch(win, top, static_cast<int>(data_index), ACS_BTEE);
          } else if (lvl_before[other] == bottom) {
            mvwaddch(win, bottom, static_cast<int>(data_index), ACS_TTEE);
          } else if (lvl_before[other] > bottom && lvl_before[other] < top) {
            mvwaddch(win, lvl_before[other], static_cast<int>(data_index), ACS_PLUS);
          } else {
            if (has_colors()) {
              wcolor_set(win, plot_line_colors[other], nullptr);
            }
            mvwaddch(win, lvl_before[other], static_cast<int>(data_index), ACS_HLINE);
            if (has_colors()) {
              wcolor_set(win, plot_line_colors[line], nullptr);
            }
          }
        }
      } else {
        mvwhline(win, lvl_now, static_cast<int>(data_index), ACS_HLINE, 1);
        for (unsigned other = 0; other < num_lines; ++other) {
          if (other != line && lvl_before[other] != lvl_now) {
            if (has_colors()) {
              wcolor_set(win, plot_line_colors[other], nullptr);
            }
            mvwaddch(win, lvl_before[other], static_cast<int>(data_index), ACS_HLINE);
            if (has_colors()) {
              wcolor_set(win, plot_line_colors[line], nullptr);
            }
          }
        }
      }
      lvl_before[line] = lvl_now;
    }
  }

  int legend_row = 0;
  for (unsigned line = 0; line < num_lines && legend_row < rows; ++line) {
    if (has_colors()) {
      wcolor_set(win, plot_line_colors[line], nullptr);
    }
    if (legend_left) {
      mvwprintw(win, legend_row, 0, "%.*s", cols, legend[line]);
    } else {
      const int length = static_cast<int>(std::strlen(legend[line]));
      mvwprintw(win, legend_row, std::max(0, cols - length), "%s", legend[line]);
    }
    ++legend_row;
  }
}

void populate_nvtop_plot_data(const std::vector<double>& gpu_history,
                              const std::vector<double>& secondary_history,
                              int refresh_ms,
                              int max_plot_cols,
                              std::vector<double>& plot_data,
                              int& visible_seconds,
                              int& plot_cols_used) {
  constexpr unsigned num_lines = 2;
  plot_cols_used = std::max(0, max_plot_cols);
  plot_data.assign(plot_cols_used, 0.0);
  if (max_plot_cols <= 0) {
    visible_seconds = 0;
    return;
  }

  const int sample_points = std::max(1, max_plot_cols / static_cast<int>(num_lines));
  const int max_visible_raw_samples = std::max(1, 60000 / std::max(100, refresh_ms));
  const int raw_samples_available = std::min<int>(std::min(gpu_history.size(), secondary_history.size()), max_visible_raw_samples);
  visible_seconds = std::min(60, std::max(1, sample_points * refresh_ms / 1000));
  if (raw_samples_available <= 0) return;

  const int buckets_to_fill = std::min(sample_points, raw_samples_available);
  const std::vector<double> util_values = resample_history(gpu_history, buckets_to_fill, raw_samples_available);
  const std::vector<double> secondary_values = resample_history(secondary_history, buckets_to_fill, raw_samples_available);
  const int sample_start = sample_points - buckets_to_fill;
  for (int sample = 0; sample < buckets_to_fill; ++sample) {
    const int offset = (sample_start + sample) * static_cast<int>(num_lines);
    plot_data[offset] = util_values[sample];
    plot_data[offset + 1] = secondary_values[sample];
  }
}

void draw_gpu_time_labels(WINDOW* window, int plot_left, int plot_cols, int visible_seconds) {
  if (plot_cols <= 0 || visible_seconds <= 0) {
    return;
  }

  char label[8];
  std::snprintf(label, sizeof(label), "%ds", visible_seconds);
  mvwprintw(window, getmaxy(window) - 1, plot_left, "%s", label);

  std::snprintf(label, sizeof(label), "%ds", visible_seconds * 3 / 4);
  mvwprintw(window, getmaxy(window) - 1, plot_left + plot_cols / 4 - static_cast<int>(std::strlen(label)) / 2, "%s", label);

  std::snprintf(label, sizeof(label), "%ds", visible_seconds / 2);
  mvwprintw(window, getmaxy(window) - 1, plot_left + plot_cols / 2 - static_cast<int>(std::strlen(label)) / 2, "%s", label);

  std::snprintf(label, sizeof(label), "%ds", visible_seconds / 4);
  mvwprintw(window, getmaxy(window) - 1, plot_left + (plot_cols * 3) / 4 - static_cast<int>(std::strlen(label)) / 2, "%s", label);
  mvwprintw(window, getmaxy(window) - 1, plot_left + plot_cols - 2, "0s");
}

void draw_gpu(WINDOW* window,
              const monitor::SystemSnapshot& snapshot,
              const std::vector<double>& gpu_history,
              const std::vector<double>& power_history,
              int refresh_ms) {
  werase(window);
  wattron(window, COLOR_PAIR(1));
  box(window, 0, 0);
  mvwprintw(window, 0, 2, " GPU ");
  wattroff(window, COLOR_PAIR(1));

  int max_y = 0;
  int max_x = 0;
  getmaxyx(window, max_y, max_x);
  const int plot_rows = std::max(4, max_y - 2);
  const int plot_left = 8;
  const int right_axis_width = 7;
  const int plot_cols = std::max(8, max_x - plot_left - right_axis_width - 1);

  mvwprintw(window, 1 + (plot_rows * 3) / 4, 1, " 25");
  mvwprintw(window, 1 + plot_rows / 4, 1, " 75");
  mvwprintw(window, 1 + plot_rows / 2, 1, " 50");
  mvwprintw(window, 1, 1, "100");
  mvwprintw(window, plot_rows, 1, "  0");
  mvwvline(window, 1, plot_left - 2, ACS_VLINE, plot_rows);

  double power_axis_max = std::max(5.0, snapshot.system_power_watts);
  for (double sample : power_history) {
    power_axis_max = std::max(power_axis_max, sample);
  }
  power_axis_max = std::ceil(power_axis_max / 5.0) * 5.0;
  std::vector<double> scaled_power_history = power_history;
  for (double& sample : scaled_power_history) {
    sample = power_axis_max > 0.0 ? (sample * 100.0 / power_axis_max) : 0.0;
  }

  std::vector<double> plot_data;
  int visible_seconds = 0;
  int plot_cols_used = 0;
  populate_nvtop_plot_data(gpu_history, scaled_power_history, refresh_ms, plot_cols, plot_data, visible_seconds, plot_cols_used);
  draw_gpu_time_labels(window, plot_left, plot_cols, visible_seconds);

  const int right_axis_x = plot_left + plot_cols + 1;
  mvwvline(window, 1, right_axis_x - 1, ACS_VLINE, plot_rows);
  auto draw_power_label = [&](int row, double value) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%4.0fW", value);
    mvwprintw(window, row, right_axis_x, "%s", buffer);
  };
  draw_power_label(1, power_axis_max);
  draw_power_label(1 + plot_rows / 4, power_axis_max * 0.75);
  draw_power_label(1 + plot_rows / 2, power_axis_max * 0.50);
  draw_power_label(1 + (plot_rows * 3) / 4, power_axis_max * 0.25);
  draw_power_label(plot_rows, 0.0);

  WINDOW* plot_window = derwin(window, plot_rows, std::max(2, plot_cols_used), 1, plot_left);
  if (!plot_window) {
    return;
  }
  werase(plot_window);

  char legend[4][32] = {};
  std::snprintf(legend[0], sizeof(legend[0]), "  GPU0 %%");
  std::snprintf(legend[1], sizeof(legend[1]), "  SOC %.1fW", snapshot.system_power_watts);
  nvtop_line_plot_port(plot_window, plot_data.size(), plot_data.data(), 2, true, legend);
  wnoutrefresh(plot_window);
  delwin(plot_window);

  (void)snapshot;
}

void draw_cpu(WINDOW* window, const monitor::SystemSnapshot& snapshot) {
  werase(window);
  wattron(window, COLOR_PAIR(1));
  box(window, 0, 0);
  mvwprintw(window, 0, 2, " CPU Cores ");
  wattroff(window, COLOR_PAIR(1));

  std::vector<const monitor::CpuCoreSnapshot*> super_cores;
  std::vector<const monitor::CpuCoreSnapshot*> performance_cores;
  std::vector<const monitor::CpuCoreSnapshot*> efficiency_cores;
  for (const auto& core : snapshot.cpu_cores) {
    if (core.cluster_type == "Super") {
      super_cores.push_back(&core);
    } else if (core.cluster_type == "Performance") {
      performance_cores.push_back(&core);
    } else if (core.cluster_type == "Efficiency") {
      efficiency_cores.push_back(&core);
    }
  }

  std::vector<std::pair<const char*, std::vector<const monitor::CpuCoreSnapshot*>*>> columns;
  if (!super_cores.empty()) columns.push_back({"S", &super_cores});
  if (!performance_cores.empty()) columns.push_back({"P", &performance_cores});
  if (!efficiency_cores.empty()) columns.push_back({"E", &efficiency_cores});
  if (columns.empty()) return;

  int max_y = 0;
  int max_x = 0;
  getmaxyx(window, max_y, max_x);
  const int inner_width = std::max(1, max_x - 2);
  const int column_width = std::max(18, inner_width / static_cast<int>(columns.size()));
  for (int column_index = 0; column_index < static_cast<int>(columns.size()); ++column_index) {
    const int x = 1 + column_index * column_width;
    const int usable_width = std::min(column_width - 1, max_x - x - 1);
    if (usable_width < 12) continue;
    mvwprintw(window, 1, x + 1, "%s", columns[column_index].first);
    const int meter_left = x + 12;
    const int meter_width = std::max(4, usable_width - 13);
    const auto& cores = *columns[column_index].second;
    for (int row = 0; row < static_cast<int>(cores.size()) && row + 2 < max_y - 1; ++row) {
      const auto& core = *cores[row];
      mvwprintw(window, row + 2, x + 1, "%3s %5.1f%%", core.label.c_str(), core.utilization_percent);
      draw_meter(window, row + 2, meter_left, meter_width, core.utilization_percent, 2);
    }
  }
}

void draw_memory(WINDOW* window, const monitor::SystemSnapshot& snapshot, const UiState& state) {
  werase(window);
  wattron(window, COLOR_PAIR(1));
  box(window, 0, 0);
  mvwprintw(window, 0, 2, " Memory ");
  wattroff(window, COLOR_PAIR(1));

  int max_x = 0;
  getmaxyx(window, std::ignore, max_x);
  const double swap_percent = snapshot.swap_total_bytes > 0
                                  ? static_cast<double>(snapshot.swap_used_bytes) * 100.0 /
                                        static_cast<double>(snapshot.swap_total_bytes)
                                  : 0.0;
  char mem_summary[96];
  char swap_summary[96];
  std::snprintf(swap_summary, sizeof(swap_summary), "%5.2f%% / %s / %s",
                swap_percent,
                human_bytes(snapshot.swap_used_bytes).c_str(),
                human_bytes(snapshot.swap_total_bytes).c_str());

  const auto pct = [&](std::uint64_t bytes) {
    return snapshot.memory_total_bytes > 0
               ? static_cast<double>(bytes) * 100.0 / static_cast<double>(snapshot.memory_total_bytes)
               : 0.0;
  };
  const std::uint64_t strict_used_bytes = snapshot.memory_wired_bytes + snapshot.memory_active_bytes;
  const double strict_used_percent = pct(strict_used_bytes);
  const double wired_percent = pct(snapshot.memory_wired_bytes);
  const double active_percent = pct(snapshot.memory_active_bytes);
  std::vector<MeterSegment> mem_segments = {
      {wired_percent, 7, A_NORMAL},
      {active_percent, 9, A_DIM},
  };
  if (state.show_cached_memory) {
    mem_segments.push_back({pct(snapshot.memory_speculative_bytes), 8, A_NORMAL});
    mem_segments.push_back({pct(snapshot.memory_purgeable_bytes), 10, A_BOLD});
    mem_segments.push_back({pct(snapshot.memory_compressed_bytes), 11, A_NORMAL});
    mem_segments.push_back({pct(snapshot.memory_inactive_bytes), 12, A_NORMAL});
  }
  std::snprintf(mem_summary, sizeof(mem_summary), "%5.2f%% / %s / %s",
                strict_used_percent,
                human_bytes(strict_used_bytes).c_str(),
                human_bytes(snapshot.memory_total_bytes).c_str());
  const int usage_width = std::max(static_cast<int>(std::strlen(mem_summary)), static_cast<int>(std::strlen(swap_summary)));
  const int meter_left = 8;
  const int meter_width = std::max(8, max_x - meter_left - usage_width - 4);
  const int usage_col = std::min(max_x - usage_width - 2, meter_left + meter_width + 2);

  mvwprintw(window, 1, 2, "Mem");
  mvwprintw(window, 2, 2, "Swp");
  draw_stacked_meter(window, 1, meter_left, meter_width, mem_segments);
  draw_meter(window, 2, meter_left, meter_width, swap_percent, 3);
  mvwprintw(window, 1, usage_col, "%s", mem_summary);
  mvwprintw(window, 2, usage_col, "%s", swap_summary);
}

void draw_processes(WINDOW* window,
                    const std::vector<ProcessRow>& rows,
                    UiState& state,
                    const ProcessViewMetrics& metrics) {
  werase(window);
  wattron(window, COLOR_PAIR(1));
  box(window, 0, 0);
  mvwprintw(window, 0, 2, " Main ");
  wattroff(window, COLOR_PAIR(1));

  const int max_y = getmaxy(window);
  const int max_x = getmaxx(window);
  if (has_colors()) wattron(window, COLOR_PAIR(4));
  mvwhline(window, 1, 1, ' ', max_x - 2);
  for (int column = 0; column < static_cast<int>(std::size(kProcessColumns)); ++column) {
    const ProcessColumn& def = kProcessColumns[column];
    const int x = process_column_offset(column);
    const std::string title = centered_text(def.title, def.width);
    if (def.sortable && state.sort == def.sort_mode) {
      if (has_colors()) wattron(window, COLOR_PAIR(5));
      else wattron(window, A_REVERSE);
      mvwprintw(window, 1, x, "%s", title.c_str());
      const int arrow_x = x + def.width - 1;
      if (arrow_x >= x && arrow_x < max_x - 1) {
        mvwaddstr(window, 1, arrow_x, state.sort_direction > 0 ? "▲" : "▼");
      }
      if (has_colors()) wattroff(window, COLOR_PAIR(5));
      else wattroff(window, A_REVERSE);
      if (has_colors()) wattron(window, COLOR_PAIR(4));
    } else {
      mvwprintw(window, 1, x, "%s", title.c_str());
    }
  }
  mvwprintw(window, 1, process_command_offset(), "Command");
  if (has_colors()) wattroff(window, COLOR_PAIR(4));

  if (rows.empty()) {
    mvwprintw(window, 3, 2, "No matching processes");
    return;
  }

  const int start = state.page * metrics.page_rows;
  const int end = std::min(start + metrics.page_rows, static_cast<int>(rows.size()));
  for (int index = start; index < end; ++index) {
    const auto& row = rows[index];
    const auto& process = *row.process;
    const int screen_row = 2 + (index - start);

    std::string command = process.command;
    if (state.tree_mode) {
      std::string prefix(row.depth * 2, ' ');
      if (row.depth > 0) {
        prefix += row.has_children ? (row.collapsed ? "+ " : "- ") : "| ";
      } else if (row.has_children) {
        prefix += row.collapsed ? "+ " : "- ";
      }
      command = prefix + command;
    }

    mvwhline(window, screen_row, 1, ' ', max_x - 2);
    if (index == metrics.selected_index) {
      if (has_colors()) wattron(window, COLOR_PAIR(15));
      else wattron(window, A_REVERSE);
    }
    mvwprintw(window, screen_row, process_column_offset(0), "%*d", kProcessColumns[0].width, process.pid);
    mvwprintw(window, screen_row, process_column_offset(1), "%-*.*s", kProcessColumns[1].width, kProcessColumns[1].width, process.user.c_str());
    mvwprintw(window, screen_row, process_column_offset(2), "%*d", kProcessColumns[2].width, process.priority);
    mvwprintw(window, screen_row, process_column_offset(3), "%*d", kProcessColumns[3].width, process.nice_value);
    mvwprintw(window, screen_row, process_column_offset(4), "%*s", kProcessColumns[4].width, compact_bytes(process.virtual_bytes).c_str());
    mvwprintw(window, screen_row, process_column_offset(5), "%*s", kProcessColumns[5].width, compact_bytes(process.resident_bytes).c_str());
    mvwprintw(window, screen_row, process_column_offset(6), "%*c", kProcessColumns[6].width, process.state);
    mvwprintw(window, screen_row, process_column_offset(7), "%*.1f", kProcessColumns[7].width, process.cpu_percent);
    mvwprintw(window, screen_row, process_column_offset(8), "%*.1f", kProcessColumns[8].width, process.memory_percent);
    mvwprintw(window, screen_row, process_column_offset(9), "%*s", kProcessColumns[9].width, format_cpu_time(process.total_cpu_time_ns).c_str());
    mvwprintw(window, screen_row, process_command_offset(), "%s",
              elide_right(command, std::max(0, max_x - process_command_offset() - 1)).c_str());
    if (index == metrics.selected_index) {
      if (has_colors()) wattroff(window, COLOR_PAIR(15));
      else wattroff(window, A_REVERSE);
    }
  }
}

void draw_key_label(WINDOW* window, int row, int& col, const char* key, const char* label, ButtonBounds* bounds, int event) {
  if (col >= getmaxx(window) - 4) {
    return;
  }
  const int start = col;
  if (has_colors()) wattron(window, COLOR_PAIR(5));
  else wattron(window, A_REVERSE);
  mvwprintw(window, row, col, "%s", key);
  if (has_colors()) wattroff(window, COLOR_PAIR(5));
  else wattroff(window, A_REVERSE);
  col += static_cast<int>(std::strlen(key));
  if (col < getmaxx(window) - 1) {
    mvwprintw(window, row, col, "%s", label);
    col += static_cast<int>(std::strlen(label));
  }
  if (col < getmaxx(window) - 1) {
    mvwprintw(window, row, col, " ");
    ++col;
  }
  if (bounds) {
    bounds->x0 = start;
    bounds->x1 = col - 1;
    bounds->event = event;
  }
}

void draw_footer(WINDOW* window, UiState& state) {
  werase(window);
  if (state.prompt_mode == PromptMode::Search) {
    mvwprintw(window, 0, 0, "search> %s", state.prompt_buffer.c_str());
  } else if (state.prompt_mode == PromptMode::Filter) {
    mvwprintw(window, 0, 0, "filter> %s", state.prompt_buffer.c_str());
  } else if (state.prompt_mode == PromptMode::Sort) {
    mvwprintw(window, 0, 0, "sort> [N]PID [P]CPU [M]EM [T]IME [A]NAME [I]nvert");
  } else if (state.prompt_mode == PromptMode::Signal) {
    mvwprintw(window, 0, 0, "signal> %s", state.prompt_buffer.c_str());
  } else if (!state.status_message.empty() &&
             std::chrono::steady_clock::now() < state.status_deadline) {
    mvwprintw(window, 0, 0, "%s", state.status_message.c_str());
  } else {
    mvwprintw(window, 0, 0, "sort=%s%s | tree=%s | filter=%s | search=%s",
              sort_mode_name(state.sort).c_str(),
              state.sort_direction > 0 ? " asc" : " desc",
              state.tree_mode ? "on" : "off",
              state.filter.empty() ? "-" : state.filter.c_str(),
              state.search.empty() ? "-" : state.search.c_str());
  }

  int col = 0;
  draw_key_label(window, 1, col, "F1", "Help", &state.footer_buttons[0], KEY_F(1));
  draw_key_label(window, 1, col, "F2", "Setup", &state.footer_buttons[1], KEY_F(2));
  draw_key_label(window, 1, col, "F3", "Search", &state.footer_buttons[2], KEY_F(3));
  draw_key_label(window, 1, col, "F4", "Filter", &state.footer_buttons[3], KEY_F(4));
  draw_key_label(window, 1, col, "F5", "Tree", &state.footer_buttons[4], KEY_F(5));
  draw_key_label(window, 1, col, "F6", "SortBy", &state.footer_buttons[5], KEY_F(6));
  draw_key_label(window, 1, col, "F7", "Nice-", &state.footer_buttons[6], KEY_F(7));
  draw_key_label(window, 1, col, "F8", "Nice+", &state.footer_buttons[7], KEY_F(8));
  draw_key_label(window, 1, col, "F9", "Kill", &state.footer_buttons[8], KEY_F(9));
  draw_key_label(window, 1, col, "F10", "Quit", &state.footer_buttons[9], KEY_F(10));
}

void draw_help_popup(const WindowLayout& layout) {
  const int height = 13;
  const int width = std::min(78, layout.cols - 4);
  const int y = std::max(1, (layout.rows - height) / 2);
  const int x = std::max(2, (layout.cols - width) / 2);
  WINDOW* popup = newwin(height, width, y, x);
  box(popup, 0, 0);
  if (has_colors()) wattron(popup, COLOR_PAIR(4));
  mvwhline(popup, 0, 1, ' ', width - 2);
  mvwprintw(popup, 0, 2, " Help ");
  if (has_colors()) wattroff(popup, COLOR_PAIR(4));
  mvwhline(popup, 1, 1, ACS_HLINE, width - 2);
  mvwprintw(popup, 3, 3, "Arrows/PgUp/PgDn/Home/End: move selection");
  mvwprintw(popup, 4, 3, "F3 or /: incremental search");
  mvwprintw(popup, 5, 3, "F4 or \\\\: incremental filter");
  mvwprintw(popup, 6, 3, "F5 or t: tree view");
  mvwprintw(popup, 7, 3, "F6 or > or .: sort menu");
  mvwprintw(popup, 8, 3, "N/P/M/T/A/I: PID, CPU, MEM, TIME, NAME, invert");
  mvwprintw(popup, 9, 3, "F7/F8 or ]/[: renice  F9 or k: send signal");
  mvwprintw(popup, 10, 3, "Mouse: click headers to sort, click rows to select");
  mvwhline(popup, height - 2, 1, ACS_HLINE, width - 2);
  mvwprintw(popup, height - 1, 3, "Esc / Enter / F1 to close");
  wnoutrefresh(popup);
  delwin(popup);
}

void draw_setup_popup(const WindowLayout& layout, const UiState& state, const monitor::AppConfig& config) {
  const int height = 11;
  const int width = std::min(56, layout.cols - 4);
  const int y = std::max(1, (layout.rows - height) / 2);
  const int x = std::max(2, (layout.cols - width) / 2);
  WINDOW* popup = newwin(height, width, y, x);
  box(popup, 0, 0);
  if (has_colors()) wattron(popup, COLOR_PAIR(4));
  mvwhline(popup, 0, 1, ' ', width - 2);
  mvwprintw(popup, 0, 2, " Setup ");
  if (has_colors()) wattroff(popup, COLOR_PAIR(4));
  mvwhline(popup, 1, 1, ACS_HLINE, width - 2);
  const char* marker_theme = state.setup_field == SetupField::Theme ? ">" : " ";
  const char* marker_cache = state.setup_field == SetupField::CachedMemory ? ">" : " ";
  const char* marker_refresh = state.setup_field == SetupField::Refresh ? ">" : " ";
  const char* marker_done = state.setup_field == SetupField::Done ? ">" : " ";
  mvwprintw(popup, 3, 3, "%s Theme         : %s", marker_theme, config.theme.c_str());
  mvwprintw(popup, 4, 3, "%s Cached memory : %s", marker_cache, state.show_cached_memory ? "on" : "off");
  mvwprintw(popup, 5, 3, "%s Refresh (ms)  : %d", marker_refresh, config.refresh_ms);
  mvwprintw(popup, 6, 3, "%s Done", marker_done);
  mvwhline(popup, height - 2, 1, ACS_HLINE, width - 2);
  mvwprintw(popup, height - 1, 3, "Up/Down select  Left/Right change  Enter/Esc/F2 close");
  wnoutrefresh(popup);
  delwin(popup);
}

bool renice_selected_process(UiState& state, int delta) {
  if (!state.selected_pid.has_value()) {
    set_status(state, "No selected process");
    return true;
  }
  errno = 0;
  const int current_nice = getpriority(PRIO_PROCESS, *state.selected_pid);
  if (errno != 0) {
    set_status(state, std::string("renice failed: ") + std::strerror(errno));
    return true;
  }
  if (setpriority(PRIO_PROCESS, *state.selected_pid, current_nice + delta) != 0) {
    set_status(state, std::string("renice failed: ") + std::strerror(errno));
    return true;
  }
  set_status(state, "Updated nice for pid " + std::to_string(*state.selected_pid));
  return true;
}

bool send_signal_to_selected_process(UiState& state, int signal_number) {
  if (!state.selected_pid.has_value()) {
    set_status(state, "No selected process");
    return true;
  }
  if (::kill(*state.selected_pid, signal_number) != 0) {
    set_status(state, std::string("signal failed: ") + std::strerror(errno));
    return true;
  }
  set_status(state, "Sent signal " + std::to_string(signal_number) + " to pid " + std::to_string(*state.selected_pid));
  return true;
}

void update_live_prompt(UiState& state, const std::vector<ProcessRow>& rows) {
  if (state.prompt_mode == PromptMode::Filter) {
    state.filter = state.prompt_buffer;
    if (!state.selected_pid.has_value()) {
      return;
    }
    bool selection_still_visible = false;
    for (const auto& row : rows) {
      if (row.process->pid == *state.selected_pid) {
        selection_still_visible = true;
        break;
      }
    }
    if (!selection_still_visible) {
      state.selected_pid.reset();
    }
  } else if (state.prompt_mode == PromptMode::Search) {
    state.search = state.prompt_buffer;
    state.search_pending = true;
  }
}

int synthesize_mouse_event(const WindowLayout& layout,
                           UiState& state,
                           const std::vector<ProcessRow>& rows,
                           const ProcessViewMetrics& metrics,
                           const MEVENT& event) {
  int process_y = 0;
  int process_x = 0;
  getbegyx(layout.processes, process_y, process_x);
  if (event.y == process_y + 1) {
    const int local_x = event.x - process_x;
    if (const auto sort = process_sort_from_header_x(local_x)) {
      if (state.sort == *sort) {
        state.sort_direction *= -1;
      } else {
        set_sort_mode(state, *sort);
      }
      set_status(state, "Sort: " + sort_mode_name(state.sort) + (state.sort_direction > 0 ? " asc" : " desc"));
      return ERR;
    }
  }

  const int row_start = process_y + 2;
  const int row_end = row_start + metrics.page_rows - 1;
  if (event.y >= row_start && event.y <= row_end) {
    const int index = state.page * metrics.page_rows + (event.y - row_start);
    if (index >= 0 && index < static_cast<int>(rows.size())) {
      state.selected_pid = rows[index].process->pid;
      return ERR;
    }
  }

  int footer_y = 0;
  int footer_x = 0;
  getbegyx(layout.footer, footer_y, footer_x);
  if (event.y == footer_y + 1) {
    const int local_x = event.x - footer_x;
    for (const auto& button : state.footer_buttons) {
      if (local_x >= button.x0 && local_x <= button.x1) {
        return button.event;
      }
    }
  }

  return ERR;
}

bool handle_input(int ch,
                  monitor::AppConfig& config,
                  UiState& state,
                  const std::vector<ProcessRow>& rows,
                  const ProcessViewMetrics& metrics) {
  if (state.show_help) {
    if (ch == 27 || ch == '\n' || ch == KEY_ENTER || ch == KEY_F(1)) {
      state.show_help = false;
    }
    return true;
  }

  if (state.show_setup) {
    if (ch == 27 || ch == KEY_F(2)) {
      state.show_setup = false;
      return true;
    }
    if (ch == KEY_UP && state.setup_field != SetupField::Theme) {
      state.setup_field = static_cast<SetupField>(static_cast<int>(state.setup_field) - 1);
      return true;
    }
    if (ch == KEY_DOWN && state.setup_field != SetupField::Done) {
      state.setup_field = static_cast<SetupField>(static_cast<int>(state.setup_field) + 1);
      return true;
    }
    if (ch == '\n' || ch == KEY_ENTER) {
      if (state.setup_field == SetupField::Done) {
        state.show_setup = false;
      }
      return true;
    }
    if (ch == KEY_LEFT || ch == KEY_RIGHT) {
      switch (state.setup_field) {
        case SetupField::Theme:
          if (config.theme == "apple") config.theme = "green";
          else if (config.theme == "green") config.theme = "mono";
          else config.theme = "apple";
          apply_theme(config);
          return true;
        case SetupField::CachedMemory:
          state.show_cached_memory = !state.show_cached_memory;
          return true;
        case SetupField::Refresh:
          config.refresh_ms = std::clamp(config.refresh_ms + (ch == KEY_RIGHT ? 250 : -250), 250, 5000);
          return true;
        case SetupField::Done:
          return true;
      }
    }
    return true;
  }

  if (state.prompt_mode != PromptMode::None) {
    if (ch == 27) {
      if (state.prompt_mode == PromptMode::Filter) {
        state.filter.clear();
        state.page = 0;
        state.selected_pid.reset();
      } else if (state.prompt_mode == PromptMode::Search) {
        state.search.clear();
      }
      state.prompt_mode = PromptMode::None;
      state.prompt_buffer.clear();
      return true;
    }
    if (ch == '\n' || ch == KEY_ENTER) {
      if (state.prompt_mode == PromptMode::Search) {
        state.search = state.prompt_buffer;
        state.search_pending = true;
      } else if (state.prompt_mode == PromptMode::Filter) {
        state.filter = state.prompt_buffer;
        state.page = 0;
        state.selected_pid.reset();
      } else if (state.prompt_mode == PromptMode::Signal) {
        const int signal_number = state.prompt_buffer.empty() ? SIGTERM : std::atoi(state.prompt_buffer.c_str());
        state.prompt_mode = PromptMode::None;
        state.prompt_buffer.clear();
        return send_signal_to_selected_process(state, std::max(1, signal_number));
      } else if (state.prompt_mode == PromptMode::Sort) {
        state.prompt_mode = PromptMode::None;
        return true;
      }
      state.prompt_mode = PromptMode::None;
      return true;
    }
    if (ch == KEY_BACKSPACE || ch == 127) {
      if (!state.prompt_buffer.empty()) {
        state.prompt_buffer.pop_back();
      }
      update_live_prompt(state, rows);
      return true;
    }
    if (state.prompt_mode == PromptMode::Sort) {
      switch (std::tolower(ch)) {
        case 'n':
          set_sort_mode(state, SortMode::Pid);
          break;
        case 'p':
          set_sort_mode(state, SortMode::Cpu);
          break;
        case 'm':
          set_sort_mode(state, SortMode::Mem);
          break;
        case 't':
          set_sort_mode(state, SortMode::Time);
          break;
        case 'a':
          set_sort_mode(state, SortMode::Name);
          break;
        case 'i':
          state.sort_direction *= -1;
          break;
        default:
          break;
      }
      state.prompt_mode = PromptMode::None;
      set_status(state, "Sort: " + sort_mode_name(state.sort) + (state.sort_direction > 0 ? " asc" : " desc"));
      return true;
    }
    if (ch >= 32 && ch <= 126) {
      state.prompt_buffer.push_back(static_cast<char>(ch));
      update_live_prompt(state, rows);
      return true;
    }
    return true;
  }

  switch (ch) {
    case 'q':
    case 'Q':
    case KEY_F(10):
      return false;
    case KEY_UP:
      move_selection(state, rows, -1);
      return true;
    case KEY_DOWN:
      move_selection(state, rows, 1);
      return true;
    case KEY_PPAGE:
    case KEY_LEFT:
      move_selection(state, rows, -metrics.page_rows);
      return true;
    case KEY_NPAGE:
    case KEY_RIGHT:
      move_selection(state, rows, metrics.page_rows);
      return true;
    case KEY_HOME:
      if (!rows.empty()) state.selected_pid = rows.front().process->pid;
      return true;
    case KEY_END:
      if (!rows.empty()) state.selected_pid = rows.back().process->pid;
      return true;
    case 'n':
    case 'N':
      set_sort_mode(state, SortMode::Pid);
      return true;
    case 'c':
    case 'p':
    case 'P':
      set_sort_mode(state, SortMode::Cpu);
      return true;
    case 'm':
    case 'M':
      set_sort_mode(state, SortMode::Mem);
      return true;
    case 'T':
      set_sort_mode(state, SortMode::Time);
      return true;
    case 'a':
    case 'A':
      set_sort_mode(state, SortMode::Name);
      return true;
    case 'i':
    case 'I':
      state.sort_direction *= -1;
      set_status(state, "Sort: " + sort_mode_name(state.sort) + (state.sort_direction > 0 ? " asc" : " desc"));
      return true;
    case '\t':
      set_sort_mode(state, cycle_sort(state.sort));
      set_status(state, "Sort: " + sort_mode_name(state.sort) + (state.sort_direction > 0 ? " asc" : " desc"));
      return true;
    case '/':
    case KEY_F(3):
      state.prompt_mode = PromptMode::Search;
      state.prompt_buffer = state.search;
      state.prompt_saved = state.search;
      return true;
    case '\\':
    case KEY_F(4):
      state.prompt_mode = PromptMode::Filter;
      state.prompt_buffer = state.filter;
      state.prompt_saved = state.filter;
      return true;
    case KEY_F(6):
    case '>':
    case '.':
      state.prompt_mode = PromptMode::Sort;
      return true;
    case KEY_F(5):
    case 't':
      state.tree_mode = !state.tree_mode;
      state.page = 0;
      set_status(state, std::string("Tree mode ") + (state.tree_mode ? "enabled" : "disabled"));
      return true;
    case KEY_F(7):
    case ']':
      return renice_selected_process(state, -1);
    case KEY_F(8):
    case '[':
      return renice_selected_process(state, 1);
    case KEY_F(9):
    case 'k':
      state.prompt_mode = PromptMode::Signal;
      state.prompt_buffer = "15";
      return true;
    case KEY_F(1):
      state.show_help = true;
      return true;
    case KEY_F(2):
      state.show_setup = true;
      return true;
    case '+':
      state.tree_mode = true;
      expand_selected(state);
      set_status(state, "Expanded selected tree node");
      return true;
    case '-':
      state.tree_mode = true;
      collapse_selected(state, rows);
      set_status(state, "Collapsed selected tree node");
      return true;
    case '*':
      state.tree_mode = true;
      toggle_all_tree_nodes(state, rows);
      set_status(state, state.collapsed_pids.empty() ? "Expanded all tree nodes" : "Collapsed all tree nodes");
      return true;
    case 'x':
      state.filter.clear();
      state.search.clear();
      state.page = 0;
      state.selected_pid.reset();
      set_status(state, "Cleared filter and search");
      return true;
    default:
      if (std::isdigit(static_cast<unsigned char>(ch))) {
        apply_pid_search(state, rows, ch);
        return true;
      }
      return true;
  }
}

monitor::SystemSnapshot apply_demo_snapshot(monitor::SystemSnapshot snapshot, int tick) {
  snapshot.capabilities.root_mode = false;
  snapshot.thermal = "Nominal";
  snapshot.ane = "N/A without root";
  snapshot.gpu_utilization_percent = 15.0 + 10.0 * std::sin(static_cast<double>(tick) / 5.0);
  snapshot.gpu_summary = std::to_string(static_cast<int>(snapshot.gpu_utilization_percent)) + "% total util";
  snapshot.system_power_watts = 18.0 + 6.0 * std::sin(static_cast<double>(tick) / 6.0);
  snapshot.gpu_memory_total_bytes = snapshot.memory_total_bytes > 0 ? snapshot.memory_total_bytes : (128ULL << 30);
  snapshot.gpu_memory_used_bytes = static_cast<std::uint64_t>(
      (5.0 + 2.0 * std::max(0.0, std::sin(static_cast<double>(tick) / 6.0))) * 1024.0 * 1024.0 * 1024.0);
  for (std::size_t i = 0; i < snapshot.cpu_cores.size(); ++i) {
    const double base = 10.0 + (i % 6) * 8.0;
    snapshot.cpu_cores[i].utilization_percent =
        std::clamp(base + 20.0 * std::sin((tick + static_cast<int>(i)) / 3.0), 0.0, 100.0);
  }

  if (snapshot.processes.empty()) {
    monitor::ProcessSnapshot window_server;
    window_server.pid = 411;
    window_server.parent_pid = 1;
    window_server.name = "WindowServer";
    window_server.command = "/System/Library/PrivateFrameworks/SkyLight.framework/WindowServer";
    window_server.user = "demo";
    window_server.cpu_percent = 3.0;
    window_server.memory_percent = 0.7;
    window_server.resident_bytes = 937ULL << 20;
    window_server.virtual_bytes = 1833ULL << 30;
    window_server.total_cpu_time_ns = 375060000000ULL;
    window_server.priority = 17;
    window_server.nice_value = 0;
    window_server.state = 'S';

    monitor::ProcessSnapshot code_renderer = window_server;
    code_renderer.pid = 81325;
    code_renderer.name = "Code Helper";
    code_renderer.command = "/Applications/Visual Studio Code.app/Contents/Frameworks/Code Helper.app/Contents/MacOS/Code Helper";
    code_renderer.cpu_percent = 2.4;
    code_renderer.memory_percent = 0.5;
    code_renderer.resident_bytes = 677ULL << 20;
    code_renderer.total_cpu_time_ns = 234170000000ULL;

    monitor::ProcessSnapshot plugin = window_server;
    plugin.pid = 81421;
    plugin.name = "Code Helper (Plugin)";
    plugin.command = "/Applications/Visual Studio Code.app/Contents/Frameworks/Code Helper (Plugin).app/Contents/MacOS/Code Helper (Plugin)";
    plugin.cpu_percent = 0.5;
    plugin.memory_percent = 3.7;
    plugin.resident_bytes = 4837ULL << 20;
    plugin.total_cpu_time_ns = 60780000000ULL;
    plugin.priority = 24;

    snapshot.processes = {window_server, code_renderer, plugin};
  }

  return snapshot;
}

void destroy_windows(WindowLayout& layout) {
  if (layout.gpu) delwin(layout.gpu);
  if (layout.cpu) delwin(layout.cpu);
  if (layout.memory) delwin(layout.memory);
  if (layout.processes) delwin(layout.processes);
  if (layout.footer) delwin(layout.footer);
  layout = {};
}

WindowLayout create_windows(int rows, int cols) {
  WindowLayout layout;
  layout.rows = rows;
  layout.cols = cols;

  const int info_height = 1;
  const int footer_height = 2;
  const int memory_height = 4;
  const int remaining = std::max(16, rows - info_height - memory_height - footer_height);
  const int preferred_top_height = 18;
  const int top_height = std::max(10, std::min(preferred_top_height, remaining - 6));
  const int process_height = std::max(6, remaining - top_height);
  const int top_y = info_height;
  const int memory_y = top_y + top_height;
  const int process_y = memory_y + memory_height;
  const int left_width = std::max(30, cols / 2);
  const int right_width = std::max(30, cols - left_width);

  layout.cpu = newwin(top_height, left_width, top_y, 0);
  layout.gpu = newwin(top_height, right_width, top_y, left_width);
  layout.memory = newwin(memory_height, cols, memory_y, 0);
  layout.processes = newwin(process_height, cols, process_y, 0);
  layout.footer = newwin(footer_height, cols, rows - footer_height, 0);
  return layout;
}

}  // namespace

int main(int argc, char** argv) {
  const RuntimeOptions options = parse_args(argc, argv);
  if (options.help) {
    print_help();
    return 0;
  }
  if (options.version) {
    print_version();
    return 0;
  }

  monitor::AppConfig config = monitor::load_config(options.config_path);
  if (options.refresh_ms.has_value()) {
    config.refresh_ms = *options.refresh_ms;
  }
  if (options.process_limit.has_value()) {
    config.process_limit = std::max(4, *options.process_limit);
  }
  if (options.theme.has_value()) {
    config.theme = *options.theme;
  }
  if (options.demo) {
    config.demo_mode = true;
  }

  std::unique_ptr<monitor::Sampler> sampler(monitor::create_darwin_sampler());
  if (!sampler) {
    std::fprintf(stderr, "failed to create sampler\n");
    return 1;
  }

  std::setlocale(LC_ALL, "");
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  nodelay(stdscr, TRUE);
  mousemask(BUTTON1_RELEASED, nullptr);
  mouseinterval(0);
  leaveok(stdscr, TRUE);
  curs_set(0);
  apply_theme(config);

  UiState state;
  std::vector<double> gpu_history;
  std::vector<double> power_history;
  int rows = 0;
  int cols = 0;
  getmaxyx(stdscr, rows, cols);
  WindowLayout layout = create_windows(rows, cols);
  bool running = true;
  int tick = 0;

  while (running) {
    monitor::SystemSnapshot snapshot = sampler->sample();
    if (config.demo_mode) {
      snapshot = apply_demo_snapshot(snapshot, tick);
    }

    gpu_history.push_back(snapshot.gpu_utilization_percent);
    power_history.push_back(snapshot.system_power_watts);
    const int max_history = std::max(60, 60000 / std::max(100, config.refresh_ms));
    if (gpu_history.size() > static_cast<std::size_t>(max_history)) {
      gpu_history.erase(gpu_history.begin(), gpu_history.begin() + (gpu_history.size() - max_history));
    }
    if (power_history.size() > static_cast<std::size_t>(max_history)) {
      power_history.erase(power_history.begin(), power_history.begin() + (power_history.size() - max_history));
    }

    getmaxyx(stdscr, rows, cols);
    if (rows != layout.rows || cols != layout.cols) {
      destroy_windows(layout);
      erase();
      layout = create_windows(rows, cols);
    }

    std::vector<ProcessRow> process_rows = build_process_rows(snapshot, state);
    apply_search_request(state, process_rows);
    ProcessViewMetrics process_metrics =
        calculate_process_metrics(process_rows, state, getmaxy(layout.processes));

    erase();
    draw_info_bar(snapshot, cols);
    draw_cpu(layout.cpu, snapshot);
    draw_gpu(layout.gpu, snapshot, gpu_history, power_history, config.refresh_ms);
    draw_memory(layout.memory, snapshot, state);
    draw_processes(layout.processes, process_rows, state, process_metrics);
    draw_footer(layout.footer, state);
    if (state.show_help) {
      draw_help_popup(layout);
    } else if (state.show_setup) {
      draw_setup_popup(layout, state, config);
    }

    wnoutrefresh(stdscr);
    wnoutrefresh(layout.cpu);
    wnoutrefresh(layout.gpu);
    wnoutrefresh(layout.memory);
    wnoutrefresh(layout.processes);
    wnoutrefresh(layout.footer);
    doupdate();

    for (int step = 0; step < 20; ++step) {
      int ch = getch();
      if (ch == KEY_MOUSE) {
        MEVENT event{};
        if (getmouse(&event) == OK && (event.bstate & BUTTON1_RELEASED)) {
          ch = synthesize_mouse_event(layout, state, process_rows, process_metrics, event);
        } else {
          ch = ERR;
        }
      }
      if (ch != ERR && !handle_input(ch, config, state, process_rows, process_metrics)) {
        running = false;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(std::max(20, config.refresh_ms / 20)));
    }
    ++tick;
  }

  destroy_windows(layout);
  endwin();
  return 0;
}
