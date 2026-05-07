#include <cassert>
#include <deque>

#include "monitor/input_parser.hpp"

namespace {

monitor::InputEvent parse(std::initializer_list<int> sequence) {
  std::deque<int> input(sequence);
  return monitor::read_input_event(
      [&]() {
        if (input.empty()) {
          return ERR;
        }
        const int ch = input.front();
        input.pop_front();
        return ch;
      },
      [&](int ch) {
        input.push_front(ch);
      });
}

}  // namespace

int main() {
  {
    monitor::InputEvent event = parse({27, 'O', 'P'});
    assert(event.key == KEY_F(1));
    assert(event.parsed_escape);
    assert(!event.has_mouse);
  }
  {
    monitor::InputEvent event = parse({27, '[', '1', '2', '~'});
    assert(event.key == KEY_F(2));
    assert(event.parsed_escape);
    assert(!event.has_mouse);
  }
  {
    monitor::InputEvent event = parse({27, '[', 'Z'});
    assert(event.key == KEY_BTAB);
    assert(event.parsed_escape);
    assert(!event.has_mouse);
  }
  {
    monitor::InputEvent event = parse({27, '[', '<', '0', ';', '5', '6', ';', '3', '1', 'm'});
    assert(event.key == KEY_MOUSE);
    assert(event.parsed_escape);
    assert(event.has_mouse);
    assert(event.mouse.x == 55);
    assert(event.mouse.y == 30);
    assert((event.mouse.bstate & BUTTON1_RELEASED) != 0);
  }
  {
    monitor::InputEvent event = parse({27, '[', '<', '0', ';', '5', '6', ';', '3', '1', 'M'});
    assert(event.key == KEY_MOUSE);
    assert(event.parsed_escape);
    assert(event.has_mouse);
    assert(event.mouse.bstate == 0);
  }
  {
    monitor::InputEvent event = parse({27, '[', '<', '1', ';', '5', '6', ';', '3', '1', 'm'});
    assert(event.key == KEY_MOUSE);
    assert(event.has_mouse);
    assert(event.mouse.bstate == 0);
  }
  return 0;
}
