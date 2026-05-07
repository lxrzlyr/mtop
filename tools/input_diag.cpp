#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <curses.h>

#include "monitor/input_parser.hpp"

namespace {

FILE* g_log = nullptr;

void log_line(const char* fmt, ...) {
  if (!g_log) {
    return;
  }
  va_list args;
  va_start(args, fmt);
  std::vfprintf(g_log, fmt, args);
  va_end(args);
  std::fputc('\n', g_log);
  std::fflush(g_log);
}

void clear_terminal_mouse_reporting() {
  static constexpr const char* sequence =
      "\033[?1000l\033[?1002l\033[?1003l\033[?1006l\033[?1015l";
  std::fwrite(sequence, 1, std::strlen(sequence), stdout);
  std::fflush(stdout);
}

void enable_terminal_mouse_reporting() {
  static constexpr const char* sequence =
      "\033[?1000h\033[?1006h";
  std::fwrite(sequence, 1, std::strlen(sequence), stdout);
  std::fflush(stdout);
}

void log_capabilities(mmask_t active_mask) {
  const char* term = std::getenv("TERM");
  log_line("TERM=%s", term ? term : "(null)");
  log_line("KEY_F1=%d KEY_F2=%d KEY_F10=%d KEY_MOUSE=%d KEY_BTAB=%d",
           KEY_F(1), KEY_F(2), KEY_F(10), KEY_MOUSE, KEY_BTAB);
  log_line("has_key(F1)=%d has_key(F2)=%d has_key(F10)=%d has_key(MOUSE)=%d has_key(BTAB)=%d",
           has_key(KEY_F(1)), has_key(KEY_F(2)), has_key(KEY_F(10)), has_key(KEY_MOUSE), has_key(KEY_BTAB));
  log_line("mousemask active=%lu", static_cast<unsigned long>(active_mask));
}

void print_help() {
  std::printf("Usage: mtop_input_diag [--log PATH] [--steps N] [--help]\n");
}

}  // namespace

int main(int argc, char** argv) {
  std::string log_path = "/Users/xiuranli/code/mtop/build/mtop-input-diag.log";
  int steps = 300;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
      print_help();
      return 0;
    }
    if (std::strcmp(argv[i], "--log") == 0 && i + 1 < argc) {
      log_path = argv[++i];
      continue;
    }
    if (std::strcmp(argv[i], "--steps") == 0 && i + 1 < argc) {
      steps = std::max(1, std::atoi(argv[++i]));
      continue;
    }
  }

  g_log = std::fopen(log_path.c_str(), "w");
  if (!g_log) {
    std::fprintf(stderr, "failed to open log: %s\n", log_path.c_str());
    return 1;
  }

  clear_terminal_mouse_reporting();
  initscr();
  cbreak();
  noecho();
  nonl();
  intrflush(stdscr, FALSE);
  keypad(stdscr, TRUE);
  meta(stdscr, TRUE);
  nodelay(stdscr, TRUE);
  mousemask(0, nullptr);
  flushinp();
  enable_terminal_mouse_reporting();
  mmask_t active_mask = mousemask(BUTTON1_RELEASED, nullptr);
  mouseinterval(0);
  monitor::configure_defined_keys();
  curs_set(0);

  log_line("diag started: %s", log_path.c_str());
  log_capabilities(active_mask);

  for (int i = 0; i < steps; ++i) {
    const monitor::InputEvent input = monitor::read_input_event(
        []() { return getch(); },
        [](int ch) { ungetch(ch); });
    const int ch = input.key;
    log_line("getch=%d", ch);
    if (input.parsed_escape) {
      log_line("parsed escape sequence key=%d", ch);
    }
    if (ch >= KEY_F(1) && ch <= KEY_F(12)) {
      log_line("function-key=F%d", ch - KEY_F(0));
    }
    if (ch == KEY_MOUSE) {
      MEVENT event{};
      if (input.has_mouse) {
        event = input.mouse;
        log_line("mouse parsed x=%d y=%d bstate=%lu", event.x, event.y, static_cast<unsigned long>(event.bstate));
      } else if (getmouse(&event) == OK) {
        log_line("mouse x=%d y=%d bstate=%lu", event.x, event.y, static_cast<unsigned long>(event.bstate));
      } else {
        log_line("mouse getmouse failed");
      }
    }
    napms(50);
  }

  endwin();
  clear_terminal_mouse_reporting();
  log_line("diag ended");
  std::fclose(g_log);
  return 0;
}
