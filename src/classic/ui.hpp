// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace classic {
struct Rect {
  int x, y, w, h;
  bool contains(int px, int py, int margin = 0) const;
};

struct AppInfo {
  std::string name, path, version, author, description;
  bool recent = false;
};

enum class Key { Up, Down, Left, Right, Enter, Escape };
enum class Action { None, Run, Close };
enum class Target { None, Selector, Run, Close, TitleClose, Row, ScrollUp, ScrollDown };

// Shared touch targets for the 320 x 528 ClassPad LCD.
struct Layout {
  static constexpr Rect selector{15, 44, 290, 28};
  static constexpr Rect run{15, 80, 140, 36};
  static constexpr Rect close{165, 80, 140, 36};
  static constexpr Rect title_close{293, 8, 20, 21};
  static constexpr int popup_y = 73, row_height = 28, max_rows = 10;
};

class State {
public:
  explicit State(std::size_t app_count);
  std::size_t count;
  int selected, first = 0;
  bool open = false;
  Target focus = Target::Selector, pressed = Target::None;
  int pressed_row = -1;
  Action action = Action::None;

  bool key(Key key, bool repeat = false);
  bool touch_down(int x, int y);
  bool touch_move(int x, int y);
  bool touch_up();
  void cancel_touch();
  int visible_rows() const;
  Rect popup() const;
  Rect row_rect(int row) const;
  bool visually_pressed(Target target) const { return pressed == target && !cancelled && !dragging; }

private:
  int down_y = 0, start_first = 0;
  bool cancelled = false, dragging = false;
  Rect press_rect{};
  void ensure_visible();
  Target hit(int x, int y, int &row) const;
  Rect target_rect(Target target, int row) const;
};

// The same renderer is exercised by the host tests and the calculator build.
struct Canvas {
  std::uint16_t *pixels;
  int width, height;
};
void render(Canvas canvas, const State &state, const std::vector<AppInfo> &apps);
} // namespace classic
