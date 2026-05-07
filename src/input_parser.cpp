#include "monitor/input_parser.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>

namespace monitor {
namespace {

int parse_defined_escape_sequence(int second, int third,
                                  const std::function<int()>& read_key) {
  if (second == 'O') {
    switch (third) {
      case 'P': return KEY_F(1);
      case 'Q': return KEY_F(2);
      case 'R': return KEY_F(3);
      case 'S': return KEY_F(4);
      default: return 27;
    }
  }

  if (second == '[') {
    if (third == 'Z') {
      return KEY_BTAB;
    }
    if (!std::isdigit(static_cast<unsigned char>(third))) {
      return 27;
    }

    std::string sequence;
    sequence.push_back(static_cast<char>(third));
    for (int guard = 0; guard < 8; ++guard) {
      const int next = read_key();
      if (next == ERR) {
        break;
      }
      sequence.push_back(static_cast<char>(next));
      if (next == '~' || std::isalpha(static_cast<unsigned char>(next))) {
        break;
      }
    }

    if (sequence == "11~") return KEY_F(1);
    if (sequence == "12~") return KEY_F(2);
    if (sequence == "13~") return KEY_F(3);
    if (sequence == "14~") return KEY_F(4);
    if (sequence == "15~") return KEY_F(5);
    if (sequence == "17~") return KEY_F(6);
    if (sequence == "18~") return KEY_F(7);
    if (sequence == "19~") return KEY_F(8);
    if (sequence == "20~") return KEY_F(9);
    if (sequence == "21~") return KEY_F(10);
  }

  return 27;
}

}  // namespace

void configure_defined_keys() {
  define_key("\033OP", KEY_F(1));
  define_key("\033OQ", KEY_F(2));
  define_key("\033OR", KEY_F(3));
  define_key("\033OS", KEY_F(4));
  define_key("\033[11~", KEY_F(1));
  define_key("\033[12~", KEY_F(2));
  define_key("\033[13~", KEY_F(3));
  define_key("\033[14~", KEY_F(4));
  define_key("\033[15~", KEY_F(5));
  define_key("\033[17~", KEY_F(6));
  define_key("\033[18~", KEY_F(7));
  define_key("\033[19~", KEY_F(8));
  define_key("\033[20~", KEY_F(9));
  define_key("\033[21~", KEY_F(10));
  define_key("\033[Z", KEY_BTAB);
}

InputEvent read_input_event(const std::function<int()>& read_key,
                            const std::function<void(int)>& unread_key) {
  InputEvent result;
  result.key = read_key();
  if (result.key != 27) {
    return result;
  }

  const int second = read_key();
  if (second == ERR) {
    return result;
  }
  const int third = read_key();
  if (third == ERR) {
    unread_key(second);
    return result;
  }

  if (second == '[' && third == '<') {
    std::string payload;
    char final = '\0';
    for (int guard = 0; guard < 32; ++guard) {
      const int next = read_key();
      if (next == ERR) {
        break;
      }
      if (next == 'M' || next == 'm') {
        final = static_cast<char>(next);
        break;
      }
      payload.push_back(static_cast<char>(next));
    }

    int button = 0;
    int x = 0;
    int y = 0;
    if (final != '\0' && std::sscanf(payload.c_str(), "%d;%d;%d", &button, &x, &y) == 3) {
      result.key = KEY_MOUSE;
      result.has_mouse = true;
      result.parsed_escape = true;
      result.mouse.x = std::max(0, x - 1);
      result.mouse.y = std::max(0, y - 1);
      result.mouse.bstate = final == 'm' ? BUTTON1_RELEASED : 0;
      if ((button & 0x3) != 0) {
        result.mouse.bstate = 0;
      }
      return result;
    }

    result.key = ERR;
    result.parsed_escape = true;
    return result;
  }

  result.key = parse_defined_escape_sequence(second, third, read_key);
  result.parsed_escape = true;
  return result;
}

}  // namespace monitor
