// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui.hpp"
#include "font.hpp"
#include "target.hpp"
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <string_view>
namespace classic {
bool Rect::contains(int px, int py, int margin) const {
  return px >= x - margin && px < x + w + margin && py >= y - margin && py < y + h + margin;
}
State::State(std::size_t app_count, std::size_t recents)
    : count(std::min(app_count, static_cast<std::size_t>(std::numeric_limits<int>::max()))),
      recent_count(std::min({count, recents, std::size_t{5}})), selected(recent_count ? 0 : -1) {
  views[0].selected = selected; views[1].selected = count ? 0 : -1;
}
int State::view_count() const { return static_cast<int>(page == Page::Recent ? recent_count : count); }
int State::visible_rows() const { return std::min(view_count(), Layout::max_rows); }
Rect State::row_rect(int index) const { return {22, 135 + (index - first) * Layout::row_height, 265, 34}; }
Rect State::thumb() const {
  const auto track = Layout::track;
  const int total = view_count();
  if (total <= Layout::max_rows) return {295, track.y, 4, track.h};
  const int height = std::max(18, track.h * Layout::max_rows / total);
  const int y = track.y + static_cast<int>(std::int64_t(first) * (track.h - height) / (total - Layout::max_rows));
  return {295, y, 4, height};
}
void State::ensure_visible() {
  if (!view_count()) { selected = -1; first = 0; return; }
  selected = std::clamp(selected, 0, view_count() - 1);
  if (selected < first) first = selected;
  if (selected >= first + visible_rows()) first = selected - visible_rows() + 1;
  first = std::clamp(first, 0, std::max(0, view_count() - Layout::max_rows));
}
void State::cancel_touch() { pressed = Target::None; pressed_row = -1; cancelled = dragging = false; }
bool State::set_page(Page next) {
  if (page == next) return false;
  views[static_cast<unsigned>(page)] = {selected, first};
  page = next;
  const auto saved = views[static_cast<unsigned>(page)];
  selected = saved.selected; first = saved.first;
  // A finger can scroll away from the selection. Restore that exact viewport;
  // arrow navigation brings the selected entry back into view.
  first = std::clamp(first, 0, std::max(0, view_count() - Layout::max_rows));
  details = false; detail_scroll = 0; cancel_touch(); return true;
}
void State::set_detail_height(int height) {
  detail_limit = std::max(0, height - Layout::detail_body.h);
  detail_scroll = std::clamp(detail_scroll, 0, detail_limit);
}
bool State::key(Key input, bool repeat) {
  if (repeat && (input == Key::Enter || input == Key::Escape || input == Key::Info)) return false;
  const bool touched = pressed != Target::None;
  cancel_touch();
  if (input == Key::Escape) {
    if (details) { details = false; return true; }
    action = Action::Close; return true;
  }
  if (input == Key::Enter) {
    if (selected >= 0) action = Action::Run;
    return action == Action::Run || touched;
  }
  if (input == Key::Info) {
    if (selected < 0) return touched;
    details = !details; detail_scroll = 0; return true;
  }
  if (details) {
    const int old = detail_scroll;
    const int delta = (input == Key::Up || input == Key::Left ? -1 : 1) *
                      (input == Key::Left || input == Key::Right ? Layout::detail_body.h : 20);
    detail_scroll = std::clamp(detail_scroll + delta, 0, detail_limit);
    return old != detail_scroll || touched;
  }
  if (input == Key::Left || input == Key::Right)
    return set_page(input == Key::Left ? Page::Recent : Page::All) || touched;
  if (!view_count()) return touched;
  const int old = selected;
  selected = std::clamp(selected + (input == Key::Down ? 1 : -1), 0, view_count() - 1);
  ensure_visible(); return old != selected || touched;
}
Target State::hit(int x, int y, int &row) const {
  row = -1;
  if (Layout::close.contains(x, y)) return details ? Target::Back : Target::Close;
  if (details) {
    if (Layout::back.contains(x, y)) return Target::Back;
    if (Layout::detail_run.contains(x, y)) return Target::Run;
    if (Layout::detail_body.contains(x, y)) return Target::DetailText;
    return Target::None;
  }
  if (Layout::recent.contains(x, y)) return Target::Recent;
  if (Layout::all.contains(x, y)) return Target::All;
  if (Layout::run.contains(x, y) && selected >= 0) return Target::Run;
  if (Layout::info.contains(x, y) && selected >= 0) return Target::Info;
  if (!Layout::list.contains(x, y)) return Target::None;
  if (view_count() > Layout::max_rows && Layout::track.contains(x, y)) return Target::Scroll;
  if (x < 22 || x >= 287 || y < 135) return Target::None;
  row = first + (y - 135) / Layout::row_height;
  if (row < first + visible_rows()) return Target::Row;
  row = -1; return Target::None;
}
Rect State::target_rect(Target target, int row) const {
  switch (target) {
  case Target::Recent: return Layout::recent;
  case Target::All: return Layout::all;
  case Target::Run: return details ? Layout::detail_run : Layout::run;
  case Target::Info: return Layout::info;
  case Target::Close: return Layout::close;
  case Target::Back: return {15, 14, 290, 42};
  case Target::Row: return row_rect(row);
  case Target::Scroll: return Layout::track;
  case Target::DetailText: return Layout::detail_body;
  default: return {};
  }
}
bool State::scroll_to(int y) {
  const auto bar = thumb();
  const int range = Layout::track.h - bar.h;
  if (range <= 0) return false;
  const int position = std::clamp(y - thumb_grab - Layout::track.y, 0, range);
  const int old = first;
  first = static_cast<int>(std::int64_t(position) * (view_count() - Layout::max_rows) / range);
  return old != first;
}
bool State::touch_down(int x, int y) {
  cancel_touch(); pressed = hit(x, y, pressed_row);
  if (pressed == Target::None) return false;
  down_x = x; down_y = y; start_first = first; start_detail = detail_scroll;
  press_rect = target_rect(pressed, pressed_row);
  if (pressed == Target::Scroll) {
    const auto bar = thumb();
    thumb_grab = y >= bar.y && y < bar.y + bar.h ? y - bar.y : bar.h / 2;
    scroll_to(y);
  }
  return true;
}
bool State::touch_move(int x, int y) {
  // Some OS lift events contain invalid coordinates; captured taps survive.
  if (pressed == Target::None || x < 0 || x >= 320 || y < 0 || y >= 528) return false;
  if (pressed == Target::Scroll) return scroll_to(y);
  if (pressed == Target::DetailText) {
    const int old = detail_scroll;
    detail_scroll = std::clamp(start_detail + down_y - y, 0, detail_limit);
    return old != detail_scroll;
  }
  if (pressed == Target::Row && !cancelled && (dragging || (std::abs(y - down_y) >= 8 && std::abs(x - down_x) < 40))) {
    const bool started = !dragging; dragging = true;
    const int old = first;
    first = std::clamp(start_first + (down_y - y) / Layout::row_height, 0, std::max(0, view_count() - Layout::max_rows));
    return started || old != first;
  }
  if (!dragging && !press_rect.contains(x, y, 8) && !cancelled) { cancelled = true; return true; }
  return false;
}
bool State::touch_up() {
  const auto target = pressed;
  const int row = pressed_row;
  const bool activate = !cancelled && !dragging;
  cancel_touch();
  if (target == Target::None) return false;
  if (!activate) return true;
  switch (target) {
  case Target::Recent: set_page(Page::Recent); break;
  case Target::All: set_page(Page::All); break;
  case Target::Row: selected = row; break;
  case Target::Run: if (selected >= 0) action = Action::Run; break;
  case Target::Info: details = true; detail_scroll = 0; break;
  case Target::Back: details = false; break;
  case Target::Close: action = Action::Close; break;
  default: break;
  }
  return true;
}
namespace {
constexpr std::uint16_t white = 0xffff, ink = 0x1082, muted = 0x630c,
                        edge = 0xbdf7, pale = 0xf7be, press = 0xdedb;
using font::Face;
class Painter {
public:
  Canvas c;
  Rect clip{0, 0, 320, 528};
  void fill(Rect r, std::uint16_t colour) {
    if (!c.pixels) return;
    const int right = std::min({c.width, r.x + r.w, clip.x + clip.w});
    const int bottom = std::min({c.height, r.y + r.h, clip.y + clip.h});
    for (int y = std::max({0, r.y, clip.y}); y < bottom; ++y)
      for (int x = std::max({0, r.x, clip.x}); x < right; ++x) c.pixels[y * c.width + x] = colour;
  }
  void round(Rect r, int radius, std::uint16_t colour) {
    radius = std::max(0, std::min({radius, r.w / 2, r.h / 2}));
    for (int y = 0; y < r.h; ++y) {
      const int dy = y < radius ? radius - y - 1 : y >= r.h - radius ? y - (r.h - radius) : 0;
      int inset = 0;
      while (inset < radius && (radius - inset - 1) * (radius - inset - 1) + dy * dy > radius * radius) ++inset;
      fill({r.x + inset, r.y + y, r.w - inset * 2, 1}, colour);
    }
  }
  void panel(Rect r, int radius, std::uint16_t colour) {
    round(r, radius, edge); round({r.x + 1, r.y + 1, r.w - 2, r.h - 2}, radius - 1, colour);
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
  static int measure(std::string_view s, const Face &face) {
    int width = 0;
    for (std::size_t i = 0; i < s.size() && width < 1000000;) {
      const auto cp = next(s, i);
      if (cp == '\n') break;
      if (cp != '\r') width += face.advance[index(cp)];
    }
    return width;
  }
  void glyph(int x, int y, unsigned ch, const Face &face, std::uint16_t colour, Rect box) {
    if (!c.pixels || y + face.height <= clip.y || y >= clip.y + clip.h) return;
    const auto n = index(ch);
    for (int yy = 0; yy < face.height; ++yy)
      for (int xx = 0; xx < 16; ++xx)
        if ((face.bits[n * face.height + yy] & (0x8000u >> xx)) && box.contains(x + xx, y + yy)) fill({x + xx, y + yy, 1, 1}, colour);
  }
  int text(Rect box, std::string_view s, const Face &face = font::body, std::uint16_t colour = ink, bool wrap = false) {
    int x = box.x, y = box.y, bottom = box.y;
    const int dots = measure("...", face);
    const bool truncated = !wrap && measure(s, face) > box.w;
    bool word_start = true;
    for (std::size_t i = 0; i < s.size() && y + face.height <= box.y + box.h && y < 1000000;) {
      if (c.pixels && y >= clip.y + clip.h) break;
      const auto begin = i;
      const auto cp = next(s, i);
      if (cp == '\r') continue;
      if (cp == '\n') { if (!wrap) break; x = box.x; y += face.line; word_start = true; continue; }
      const int advance = face.advance[index(cp)];
      if (wrap && word_start && cp != ' ') {
        auto end = s.find_first_of(" \r\n", begin);
        if (end == std::string_view::npos) end = s.size();
        const int word_width = measure(s.substr(begin, end - begin), face);
        if (x != box.x && word_width <= box.w && x + word_width > box.x + box.w) { x = box.x; y += face.line; }
        if (y + face.height > box.y + box.h) break;
      }
      if (!wrap && truncated && x + advance > box.x + box.w - dots) {
        for (int dot = 0; dot < 3; ++dot) { glyph(x, y, '.', face, colour, box); x += face.advance[index('.')]; }
        bottom = y + face.line; break;
      }
      if (x + advance > box.x + box.w) {
        if (!wrap) break;
        x = box.x; y += face.line;
        if (y + face.height > box.y + box.h) break;
      }
      if (wrap && cp == ' ' && x == box.x) { word_start = true; continue; }
      glyph(x, y, cp, face, colour, box); x += advance; bottom = y + face.line; word_start = cp == ' ';
    }
    return bottom;
  }
  void centred(Rect r, std::string_view label, const Face &face = font::body, std::uint16_t colour = ink) {
    const int width = std::min(r.w - 8, measure(label, face));
    text({r.x + (r.w - width) / 2, r.y + (r.h - face.height) / 2, width, face.height}, label, face, colour);
  }
  void button(Rect r, std::string_view label, bool primary, bool down, bool enabled = true) {
    if (primary) round(r, 14, down ? muted : ink); else panel(r, 14, down ? press : pale);
    centred(r, label, font::body, primary ? (enabled ? white : edge) : (enabled ? ink : muted));
  }
};
int draw_details(Painter &p, const AppInfo &app, int scroll) {
  const int start = Layout::detail_body.y - scroll;
  int y = p.text({15, start, 290, 1000000}, app.name, font::heading, ink, true) + 10;
  if (!app.version.empty()) y = p.text({15, y, 290, 1000000}, "Version " + app.version, font::small, muted, true) + 4;
  if (!app.author.empty()) y = p.text({15, y, 290, 1000000}, "By " + app.author, font::body, muted, true) + 4;
  y = p.text({15, y + 8, 290, 1000000}, app.path, font::small, muted, true) + 14;
  p.fill({15, y, 290, 1}, edge);
  return p.text({15, y + 12, 290, 1000000}, app.description, font::body, ink, true) + 8 - start;
}
std::string_view extension(std::string_view path) {
  const auto dot = path.find_last_of('.');
  if (dot == std::string_view::npos) return {};
  const auto suffix = path.substr(dot);
  if (target::equals(suffix, ".hh3")) return "HH3";
  if (target::equals(suffix, ".hhk")) return "HHK";
  if (target::equals(suffix, ".bin")) return "BIN";
  return {};
}
}
int details_height(const AppInfo &app) { Painter measure{{nullptr, 0, 0}}; return draw_details(measure, app, 0); }
void render(Canvas canvas, const State &s, const std::vector<AppInfo> &apps) {
  Painter p{canvas}; p.fill({0, 0, canvas.width, canvas.height}, white);
  const bool has_app = s.selected >= 0 && static_cast<std::size_t>(s.selected) < apps.size();
  p.panel(Layout::close, 15, s.visually_pressed(s.details ? Target::Back : Target::Close) ? press : white);
  for (int i = 0; i < 10; ++i) { p.fill({279 + i, 30 + i, 2, 2}, ink); p.fill({288 - i, 30 + i, 2, 2}, ink); }
  if (s.details && has_app) {
    p.button(Layout::back, "< Back", false, s.visually_pressed(Target::Back));
    p.clip = Layout::detail_body; draw_details(p, apps[s.selected], s.detail_scroll); p.clip = {0, 0, 320, 528};
    p.button(Layout::detail_run, "Run app", true, s.visually_pressed(Target::Run));
  } else {
    p.text({15, 35, 225, 24}, "Hollyhock", font::heading);
    p.fill({15, 70, 290, 1}, edge);
    p.panel(Layout::tabs, 17, pale);
    p.round(s.page == Page::Recent ? Layout::recent : Layout::all, 14, ink);
    p.centred(Layout::recent, "Recent", font::body, s.page == Page::Recent ? white : ink);
    p.centred(Layout::all, "All apps", font::body, s.page == Page::All ? white : ink);
    p.panel(Layout::list, 14, pale);
    // Drawing cost depends on the viewport, not the total library size.
    for (int row = s.first; row < s.first + s.visible_rows() && static_cast<std::size_t>(row) < apps.size(); ++row) {
      const auto r = s.row_rect(row);
      const bool selected = row == s.selected;
      const bool pressed = s.visually_pressed(Target::Row) && row == s.pressed_row;
      if (selected || pressed) p.round(r, 11, selected ? ink : press);
      const auto colour = selected ? white : ink;
      p.text({r.x + 9, r.y + 8, 218, 20}, apps[row].name, font::body, colour);
      p.text({r.x + r.w - 31, r.y + 10, 28, 16}, extension(apps[row].path), font::small, colour);
    }
    if (s.view_count() > Layout::max_rows) {
      p.round({295, Layout::track.y, 4, Layout::track.h}, 2, edge); p.round(s.thumb(), 2, muted);
    }
    if (!s.view_count()) {
      if (s.page == Page::Recent)
        p.text({31, 151, 244, 190}, "No recent apps yet.\n\nOpen All apps to launch one. Your last five apps will appear here.", font::body, muted, true);
      else p.text({31, 151, 244, 200}, target::empty_message, font::body, muted, true);
    }
    p.fill({15, 387, 290, 1}, edge);
    if (has_app) {
      p.text({15, 401, 290, 20}, apps[s.selected].name);
      p.text({15, 425, 290, 16}, apps[s.selected].path, font::small, muted);
    }
    p.button(Layout::run, "Run app", true, s.visually_pressed(Target::Run), has_app);
    p.button(Layout::info, "Info (0)", false, s.visually_pressed(Target::Info), has_app);
  }
  p.text({15, 503, 110, 16}, s.details ? "Clear: back" : std::to_string(s.view_count()) + " apps", font::small, muted);
  p.text({132, 503, 174, 16}, target::footer, font::small, muted);
}
} // namespace classic
