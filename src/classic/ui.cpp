// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui.hpp"
#include "target.hpp"
#include "font.hpp"
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <string_view>

namespace classic {
bool Rect::contains(int px, int py, int margin) const {
  return px >= x - margin && px < x + w + margin &&
         py >= y - margin && py < y + h + margin;
}

State::State(std::size_t app_count)
    : count(std::min(app_count, static_cast<std::size_t>(std::numeric_limits<int>::max()))),
      selected(count ? 0 : -1) {}
int State::visible_rows() const { return std::min(static_cast<int>(count), Layout::max_rows); }
Rect State::popup() const { return {15, Layout::popup_y, 290, visible_rows() * Layout::row_height + 2}; }
Rect State::row_rect(int row) const {
  return {16, Layout::popup_y + 1 + (row - first) * Layout::row_height, 265, Layout::row_height};
}
void State::ensure_visible() {
  if (selected < first) first = selected;
  if (selected >= first + visible_rows()) first = selected - visible_rows() + 1;
  first = std::clamp(first, 0, std::max(0, static_cast<int>(count) - visible_rows()));
}
void State::cancel_touch() {
  pressed = Target::None;
  pressed_row = -1;
  cancelled = false;
  dragging = false;
}

bool State::key(Key input, bool repeat) {
  // Key-up is discarded by the platform adapter. Held EXE/Clear must not
  // turn a single confirmation into two separate actions.
  if (repeat && (input == Key::Enter || input == Key::Escape)) return false;
  const bool had_touch = pressed != Target::None;
  cancel_touch();
  switch (input) {
  case Key::Escape:
    if (open) { open = false; return true; }
    action = Action::Close;
    return true;
  case Key::Enter:
    if (open) { open = false; return true; }
    if (focus == Target::Close) action = Action::Close;
    else if (selected >= 0) action = Action::Run;
    return action != Action::None;
  case Key::Up:
  case Key::Down: {
    if (!count) return false;
    const auto old = selected;
    selected = std::clamp(selected + (input == Key::Down ? 1 : -1), 0, static_cast<int>(count) - 1);
    ensure_visible();
    return old != selected || had_touch;
  }
  case Key::Left:
  case Key::Right:
    if (open) {
      if (!count) return false;
      selected = std::clamp(selected + (input == Key::Right ? visible_rows() : -visible_rows()),
                            0, static_cast<int>(count) - 1);
      ensure_visible();
    } else {
      int slot = focus == Target::Selector ? 0 : focus == Target::Run ? 1 : 2;
      slot = (slot + (input == Key::Right ? 1 : 2)) % 3;
      focus = slot == 0 ? Target::Selector : slot == 1 ? Target::Run : Target::Close;
    }
    return true;
  }
  return false;
}

Target State::hit(int x, int y, int &row) const {
  row = -1;
  if (Layout::title_close.contains(x, y)) return Target::TitleClose;
  if (Layout::selector.contains(x, y)) return Target::Selector;
  if (open) {
    const auto box = popup();
    if (!box.contains(x, y)) return Target::None;
    if (x >= 282 && count > static_cast<std::size_t>(visible_rows())) {
      if (y < box.y + 28) return Target::ScrollUp;
      if (y >= box.y + box.h - 28) return Target::ScrollDown;
      return Target::None;
    }
    row = first + (y - box.y - 1) / Layout::row_height;
    if (row >= first && row < first + visible_rows() && row < static_cast<int>(count)) return Target::Row;
    return Target::None;
  }
  if (Layout::run.contains(x, y) && count) return Target::Run;
  if (Layout::close.contains(x, y)) return Target::Close;
  return Target::None;
}
Rect State::target_rect(Target target, int row) const {
  switch (target) {
  case Target::Selector: return Layout::selector;
  case Target::Run: return Layout::run;
  case Target::Close: return Layout::close;
  case Target::TitleClose: return Layout::title_close;
  case Target::Row: return row_rect(row);
  case Target::ScrollUp: return {282, popup().y, 23, 28};
  case Target::ScrollDown: return {282, popup().y + popup().h - 28, 23, 28};
  default: return {};
  }
}
bool State::touch_down(int x, int y) {
  cancel_touch();
  pressed = hit(x, y, pressed_row);
  if (open && pressed == Target::None) { open = false; return true; }
  if (pressed == Target::None) return false;
  press_rect = target_rect(pressed, pressed_row);
  down_y = y;
  start_first = first;
  if (pressed == Target::Selector || pressed == Target::Run || pressed == Target::Close) focus = pressed;
  return true;
}
bool State::touch_move(int x, int y) {
  if (pressed == Target::None || x < 0 || x >= 320 || y < 0 || y >= 528) return false;
  if (open && pressed == Target::Row && (dragging || std::abs(y - down_y) >= 12)) {
    dragging = true;
    const auto old = first;
    first = std::clamp(start_first + (down_y - y) / Layout::row_height,
                       0, std::max(0, static_cast<int>(count) - visible_rows()));
    return old != first;
  }
  if (!press_rect.contains(x, y, 8) && !cancelled) {
    cancelled = true;
    const bool changed = pressed != Target::Row;
    return changed;
  }
  return false;
}
bool State::touch_up() {
  const auto target = pressed;
  const auto row = pressed_row;
  const auto activate = !cancelled && !dragging;
  cancel_touch();
  if (target == Target::None) return false;
  // Lift coordinates are deliberately not used for hit testing. The contact
  // was captured on DOWN and cancelled only by an actual out-of-bounds MOVE.
  if (!activate) return true;
  switch (target) {
  case Target::Selector:
    if (count) { open = !open; ensure_visible(); }
    break;
  case Target::Row: selected = row; open = false; focus = Target::Selector; break;
  case Target::Run: action = Action::Run; break;
  case Target::Close:
  case Target::TitleClose: action = Action::Close; break;
  case Target::ScrollUp: first = std::max(0, first - 1); break;
  case Target::ScrollDown: first = std::min(std::max(0, static_cast<int>(count) - visible_rows()), first + 1); break;
  default: break;
  }
  return true;
}

namespace {
constexpr std::uint16_t white = 0xffff, black = 0x0000, body = 0xf7be,
                        edge = 0x7bef, shadow = 0xbdf7, title = 0x6b4d, blue = 0x0010;
class Painter {
public:
  Canvas c;
  void fill(Rect r, std::uint16_t colour) {
    const int right = std::min(c.width, r.x + r.w), bottom = std::min(c.height, r.y + r.h);
    for (int y = std::max(0, r.y); y < bottom; ++y)
      for (int x = std::max(0, r.x); x < right; ++x) c.pixels[y * c.width + x] = colour;
  }
  void border(Rect r, std::uint16_t colour) {
    fill({r.x, r.y, r.w, 1}, colour); fill({r.x, r.y + r.h - 1, r.w, 1}, colour);
    fill({r.x, r.y, 1, r.h}, colour); fill({r.x + r.w - 1, r.y, 1, r.h}, colour);
  }
  void bevel(Rect r, bool down) {
    fill(r, body); border(r, edge);
    const auto light = down ? shadow : white, dark = down ? white : shadow;
    fill({r.x + 1, r.y + 1, r.w - 2, 2}, light);
    fill({r.x + 1, r.y + 1, 2, r.h - 2}, light);
    fill({r.x + 2, r.y + r.h - 3, r.w - 3, 2}, dark);
    fill({r.x + r.w - 3, r.y + 2, 2, r.h - 3}, dark);
  }
  void dotted(Rect r) {
    for (int x = r.x; x < r.x + r.w; x += 2) { fill({x, r.y, 1, 1}, black); fill({x, r.y + r.h - 1, 1, 1}, black); }
    for (int y = r.y; y < r.y + r.h; y += 2) { fill({r.x, y, 1, 1}, black); fill({r.x + r.w - 1, y, 1, 1}, black); }
  }
  static unsigned next(std::string_view s, std::size_t &i) {
    const auto a = static_cast<unsigned char>(s[i++]);
    if (a < 128) return a;
    if (a >= 0xc2 && a <= 0xc3 && i < s.size()) {
      const auto b = static_cast<unsigned char>(s[i]);
      if ((b & 0xc0) == 0x80) { ++i; return ((a & 0x1f) << 6) | (b & 0x3f); }
    }
    while (i < s.size() && (static_cast<unsigned char>(s[i]) & 0xc0) == 0x80) ++i;
    return '?';
  }
  static unsigned index(unsigned ch) {
    if (ch >= 32 && ch <= 126) return ch - 32;
    if (ch >= 160 && ch <= 255) return ch - 160 + 95;
    return '?' - 32;
  }
  static int measure(std::string_view s) {
    int w = 0;
    for (std::size_t i = 0; i < s.size();) w += font::advance[index(next(s, i))];
    return w;
  }
  void glyph(int x, int y, unsigned ch, std::uint16_t colour, Rect clip) {
    const auto n = index(ch);
    for (int yy = 0; yy < font::height; ++yy)
      for (int xx = 0; xx < font::width; ++xx)
        if ((font::bits[n][yy] & (0x8000u >> xx)) && clip.contains(x + xx, y + yy)) fill({x + xx, y + yy, 1, 1}, colour);
  }
  void text(Rect box, std::string_view s, std::uint16_t colour = black, bool wrap = false) {
    int x = box.x, y = box.y;
    for (std::size_t i = 0; i < s.size() && y + font::height <= box.y + box.h;) {
      const auto cp = next(s, i);
      if (cp == '\r') continue;
      if (cp == '\n') { if (!wrap) break; x = box.x; y += font::line; continue; }
      const int adv = font::advance[index(cp)];
      if (x + adv > box.x + box.w) {
        if (!wrap) { text({std::max(box.x, box.x + box.w - 16), y, 16, font::height}, "...", colour); break; }
        x = box.x; y += font::line;
        if (y + font::height > box.y + box.h) break;
      }
      if (wrap && cp != ' ' && (i <= 1 || s[i - 2] == ' ')) {
        // Start a word on the next line when it fits there as a whole.
        auto end = s.find_first_of(" \r\n", i);
        if (end == std::string_view::npos) end = s.size();
        const int remaining = measure(s.substr(i, end - i));
        if (x != box.x && adv + remaining <= box.w && x + adv + remaining > box.x + box.w) { x = box.x; y += font::line; }
        if (y + font::height > box.y + box.h) break;
      }
      glyph(x, y, cp, colour, box); x += adv;
    }
  }
  void centred(Rect r, std::string_view label, std::uint16_t colour = black) {
    const int w = measure(label);
    text({r.x + (r.w - w) / 2, r.y + (r.h - font::height) / 2, w, font::height}, label, colour);
  }
  void arrow(int x, int y, bool up, std::uint16_t colour) {
    for (int line = 0; line < 6; ++line) {
      const int w = up ? line * 2 + 1 : 11 - line * 2;
      fill({x + (11 - w) / 2, y + line, w, 1}, colour);
    }
  }
};
}

void render(Canvas canvas, const State &s, const std::vector<AppInfo> &apps) {
  Painter p{canvas};
  p.fill({0, 0, canvas.width, canvas.height}, white);
  p.bevel({2, 1, 317, 501}, false);
  p.fill({6, 5, 309, 27}, title);
  p.border({5, 4, 311, 29}, black);
  p.text({15, 9, 270, 20}, "Hollyhock Launcher", white);
  p.bevel(Layout::title_close, s.visually_pressed(Target::TitleClose));
  p.centred(Layout::title_close, "x");
  p.fill({7, 34, 307, 462}, white);

  p.bevel(Layout::selector, true);
  p.fill({18, 47, 261, 22}, white);
  p.bevel({280, 45, 24, 26}, s.open);
  p.arrow(287, 55, false, edge);
  const bool has_app = s.selected >= 0 && static_cast<std::size_t>(s.selected) < apps.size();
  p.text({26, 49, 248, 20}, has_app ? apps[s.selected].name : "No apps found");

  p.bevel(Layout::run, s.visually_pressed(Target::Run));
  p.centred(Layout::run, "Run", has_app ? black : edge);
  p.bevel(Layout::close, s.visually_pressed(Target::Close));
  p.centred(Layout::close, "Close");
  const auto fr = s.focus == Target::Run ? Layout::run : s.focus == Target::Close ? Layout::close : Layout::selector;
  if (!s.open) p.dotted({fr.x + 4, fr.y + 4, fr.w - (s.focus == Target::Selector ? 33 : 8), fr.h - 8});

  if (has_app) {
    const auto &a = apps[s.selected];
    std::string info = a.name;
    if (!a.version.empty() || !a.author.empty()) {
      info += "\n(";
      if (!a.version.empty()) info += "version " + a.version;
      if (!a.author.empty()) info += (a.version.empty() ? "by " : " by ") + a.author;
      info += ")";
    }
    info += "\n(from " + a.path + ")";
    if (!a.description.empty()) info += "\n\n" + a.description;
    p.text({15, 127, 288, 360}, info, black, true);
  } else {
    p.text({15, 127, 288, 360}, target::empty_message, black, true);
  }
  p.fill({0, 503, 320, 2}, shadow);
  p.text({8, 508, 120, 20}, target::name);
  p.text({205, 508, 108, 20}, target::footer);

  if (!s.open) return;
  const auto box = s.popup();
  p.fill({box.x + 3, box.y + 3, box.w, box.h}, shadow);
  p.fill(box, white); p.border(box, edge);
  for (int row = s.first; row < s.first + s.visible_rows() && static_cast<std::size_t>(row) < apps.size(); ++row) {
    const auto r = s.row_rect(row);
    const bool active = row == s.selected || (s.pressed == Target::Row && row == s.pressed_row);
    if (active) p.fill(r, blue);
    if (row > 0 && !apps[row].recent && apps[row - 1].recent)
      p.fill({r.x + 2, r.y, r.w - 4, 1}, edge);
    const int inset = apps[row].recent ? 22 : 8;
    if (apps[row].recent) p.text({r.x + 6, r.y + 4, 12, 20}, "*", active ? white : black);
    p.text({r.x + inset, r.y + 4, r.w - inset - 5, 20}, apps[row].name, active ? white : black);
  }
  if (s.count > static_cast<std::size_t>(s.visible_rows())) {
    p.fill({282, box.y + 1, 22, box.h - 2}, body);
    p.bevel({282, box.y + 1, 22, 27}, false);
    p.arrow(287, box.y + 11, true, edge);
    p.bevel({282, box.y + box.h - 28, 22, 27}, false);
    p.arrow(287, box.y + box.h - 18, false, edge);
    const int track = box.h - 58;
    const int thumb = std::max(12, track * s.visible_rows() / static_cast<int>(s.count));
    const int thumb_y = box.y + 29 + (track - thumb) * s.first / (static_cast<int>(s.count) - s.visible_rows());
    p.bevel({284, thumb_y, 18, thumb}, false);
  }
}
} // namespace classic
