#pragma once

#include <functional>

#include <curses.h>

namespace monitor {

struct InputEvent {
  int key = ERR;
  bool has_mouse = false;
  MEVENT mouse{};
  bool parsed_escape = false;
};

void configure_defined_keys();
InputEvent read_input_event(const std::function<int()>& read_key,
                            const std::function<void(int)>& unread_key);

}  // namespace monitor
