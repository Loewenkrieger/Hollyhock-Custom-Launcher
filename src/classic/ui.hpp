// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace classic {
struct Rect { int x, y, w, h; bool contains(int px, int py, int margin = 0) const; };
struct AppInfo { std::string name, path, version, author, description; bool recent = false; };
enum class Key { Up, Down, Left, Right, Enter, Escape, Info };
enum class Action { None, Run, Close };
enum class Page { Recent, All };
enum class Target { None, Recent, All, Run, Info, Close, Back, Row, Scroll, DetailText };
struct Layout {
  static constexpr Rect close{263, 14, 42, 42}, back{15, 14, 108, 36};
  static constexpr Rect tabs{15, 83, 290, 34};
  static constexpr Rect recent{18, 86, 140, 28}, all{160, 86, 142, 28};
  static constexpr Rect list{15, 128, 290, 248}, track{288, 135, 14, 234};
  static constexpr Rect run{15, 449, 206, 42}, info{229, 449, 76, 42};
  static constexpr Rect detail_body{15, 70, 290, 363}, detail_run{15, 449, 290, 42};
  static constexpr int row_height = 38, max_rows = 6;
};
// Indices refer to the adapter's original entries. Recent is a bounded prefix;
// All retains every entry without allocating a second list of app metadata.
class State {
public:
  explicit State(std::size_t app_count, std::size_t recent_count = 0);
  std::size_t count, recent_count;
  Page page = Page::Recent;
  int selected, first = 0;
  bool details = false;
  int detail_scroll = 0;
  Target pressed = Target::None;
  int pressed_row = -1;
  Action action = Action::None;
  bool key(Key key, bool repeat = false);
  bool touch_down(int x, int y);
  bool touch_move(int x, int y);
  bool touch_up();
  void cancel_touch();
  bool set_page(Page next);
  int view_count() const;
  int visible_rows() const;
  Rect row_rect(int index) const;
  Rect thumb() const;
  void set_detail_height(int height);
  bool visually_pressed(Target target) const { return pressed == target && !cancelled && !dragging; }
private:
  struct View { int selected = -1, first = 0; };
  std::array<View, 2> views{};
  int down_x = 0, down_y = 0, start_first = 0, start_detail = 0, thumb_grab = 0;
  int detail_limit = 0;
  bool cancelled = false, dragging = false;
  Rect press_rect{};
  void ensure_visible();
  Target hit(int x, int y, int &row) const;
  Rect target_rect(Target target, int row) const;
  bool scroll_to(int y);
};
struct Canvas { std::uint16_t *pixels; int width, height; };
int details_height(const AppInfo &app);
void render(Canvas canvas, const State &state, const std::vector<AppInfo> &apps);
} // namespace classic
