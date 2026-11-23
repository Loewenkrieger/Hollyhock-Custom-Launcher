// SPDX-License-Identifier: GPL-3.0-or-later
#include "variant_fixture.hpp"
#include "classic/ui.hpp"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>
using namespace classic;
static int passed;
static void pass(const char *name) { ++passed; std::cout << "PASS " << name << '\n'; }
static void tap(State &s, int x, int y) { s.touch_down(x, y); s.touch_up(); }
static void row(State &s, int index) { const auto r = s.row_rect(index); tap(s, r.x + 30, r.y + 15); }
static std::vector<AppInfo> sample(int count) {
  std::vector<AppInfo> apps;
  const char *names[] = {"Tetris", "Snake", "2048", "Mandelbrot", "Text Editor"};
  for (int i = 0; i < count; ++i)
    apps.push_back({i < 5 ? names[i] : "Application " + std::to_string(i + 1),
      "\\fls0\\app" + std::to_string(i + 1) + APP_EXT, "1.0.0", "Example app",
      "An example application for the local Compact beta.", i < 5});
  return apps;
}
static std::vector<std::uint16_t> pixels(const State &s, const std::vector<AppInfo> &apps) {
  std::vector<std::uint16_t> out(320 * 528); render({out.data(), 320, 528}, s, apps); return out;
}
static void ppm(const std::string &path, const State &s, const std::vector<AppInfo> &apps) {
  const auto data = pixels(s, apps);
  std::ofstream file(path, std::ios::binary); file << "P6\n320 528\n255\n";
  for (auto pixel : data) {
    const char rgb[] = {static_cast<char>(((pixel >> 11) & 31) * 255 / 31),
      static_cast<char>(((pixel >> 5) & 63) * 255 / 63), static_cast<char>((pixel & 31) * 255 / 31)};
    file.write(rgb, 3);
  }
}
int main(int argc, char **argv) {
  { State s(10000, 10000); assert(s.page == Page::Recent && s.view_count() == 5 && s.visible_rows() == 5 && s.selected == 0);
    s.key(Key::Right); assert(s.view_count() == 10000 && s.visible_rows() == 6); pass("Recent starts first, caps at five and never truncates All apps"); }
  { State s(9); assert(s.selected == -1); s.key(Key::Enter); tap(s, 40, 466); assert(s.action == Action::None);
    s.key(Key::Right); assert(s.selected == 0 && s.view_count() == 9); s.key(Key::Enter); assert(s.action == Action::Run); pass("empty history keeps Recent empty; Right and EXE open the full library"); }
  { State s(0, 8); tap(s, 40, 466); s.key(Key::Info); s.key(Key::Enter); assert(!s.details && s.action == Action::None);
    tap(s, 283, 34); assert(s.action == Action::Close); pass("empty libraries cannot launch and the close control remains active"); }
  { State s(10, 2); tap(s, 222, 98); assert(s.page == Page::All); row(s, 3); assert(s.selected == 3 && s.action == Action::None);
    tap(s, 90, 98); assert(s.page == Page::Recent && s.selected == 0); pass("tab taps and row taps select without launching"); }
  { State s(1000, 5); s.key(Key::Right); for (int i = 0; i < 999; ++i) s.key(Key::Down, true);
    assert(s.selected == 999 && s.first == 994); s.key(Key::Down); assert(s.selected == 999);
    s.key(Key::Left); s.key(Key::Down); s.key(Key::Right); assert(s.selected == 999 && s.first == 994);
    s.key(Key::Left); assert(s.selected == 1); pass("1000-app keyboard navigation reaches the end and restores each page's selection and viewport"); }
  { State s(10000, 5); s.key(Key::Right); const auto bar = s.thumb(); s.touch_down(296, bar.y + 5); s.touch_move(296, 510); s.touch_up();
    assert(s.first == 9994 && s.selected == 0); row(s, 9999); assert(s.selected == 9999);
    s.key(Key::Left); s.key(Key::Right); assert(s.selected == 9999 && s.first == 9994);
    s.key(Key::Enter); assert(s.action == Action::Run); pass("dragging the scroll thumb reaches and launches the final app among 10000 entries"); }
  { State s(1000, 5); s.key(Key::Right); s.touch_down(100, 300); s.touch_move(100, 140); s.touch_up();
    assert(s.first == 4 && s.selected == 0 && s.action == Action::None); s.key(Key::Left); s.key(Key::Right);
    assert(s.first == 4 && s.selected == 0); pass("list dragging scrolls without selecting and page changes preserve an unselected viewport"); }
  { State s(1000, 5); s.key(Key::Right); s.touch_down(100, 180); assert(s.visually_pressed(Target::Row));
    assert(s.touch_move(100, 169)); assert(!s.visually_pressed(Target::Row)); s.touch_up(); assert(s.selected == 0); pass("a short drag clears press feedback before a row scroll completes"); }
  { State s(1000, 5); s.key(Key::Right); s.touch_down(100, 170); s.touch_move(100, 90); s.touch_up();
    assert(s.first == 2 && s.selected == 0); pass("swipes can start in the visual spacing between list rows"); }
  { State s(20, 5); s.key(Key::Right); s.touch_down(100, 180); assert(s.touch_move(310, 180)); s.touch_up();
    assert(s.selected == 0 && s.action == Action::None); pass("horizontal movement away from a row cancels selection"); }
  { State s(2, 2); s.touch_down(50, 466); assert(s.action == Action::None); s.touch_move(-1, -1); s.touch_up();
    assert(s.action == Action::Run && !s.touch_up()); pass("short DOWN-UP Run taps survive invalid lift coordinates and launch once"); }
  { State s(2, 2); s.touch_down(50, 466); s.touch_move(250, 300); assert(!s.visually_pressed(Target::Run)); s.touch_up();
    assert(s.action == Action::None); pass("moving away cancels Run and its pressed appearance"); }
  { State s(10, 5); s.key(Key::Right); s.key(Key::Down); assert(!s.key(Key::Right, true) && s.selected == 1);
    s.key(Key::Info); assert(s.details); assert(!s.key(Key::Info, true)); s.key(Key::Escape); assert(!s.details);
    assert(!s.key(Key::Escape, true) && s.action == Action::None); s.key(Key::Escape); assert(s.action == Action::Close);
    pass("held page, Info and Clear keys do not reset views or trigger duplicate actions"); }
  { State s(1, 1); assert(!s.key(Key::Enter, true)); assert(s.action == Action::None); s.key(Key::Enter); assert(s.action == Action::Run);
    pass("held EXE cannot launch until a genuine press"); }
  { State s(10, 5); row(s, 2); tap(s, 260, 467); assert(s.details && s.selected == 2);
    s.set_detail_height(2000); s.key(Key::Down); assert(s.detail_scroll == 20); s.key(Key::Right); assert(s.detail_scroll == 383);
    s.touch_down(90, 300); s.touch_move(90, 100); s.touch_up(); assert(s.detail_scroll == 583);
    s.key(Key::Escape); assert(!s.details && s.page == Page::Recent && s.selected == 2);
    pass("Info supports keyboard and stylus scrolling and returns to the original selection"); }
  { State s(1, 1); s.key(Key::Info); tap(s, 283, 34); assert(!s.details && s.action == Action::None);
    s.key(Key::Info); tap(s, 55, 30); assert(!s.details); s.key(Key::Info); s.key(Key::Enter); assert(s.action == Action::Run);
    pass("detail X and Back return to the list; EXE still launches the chosen app"); }
  { State s(std::numeric_limits<std::size_t>::max(), 9); s.key(Key::Right); s.touch_down(296, 140); s.touch_move(296, 510); s.touch_up();
    assert(s.first == std::numeric_limits<int>::max() - 6); pass("scroll arithmetic remains bounded at the largest supported app count"); }
  { auto apps = sample(10000); State s(apps.size(), 5); const auto before = pixels(s, apps);
    for (int i = 5; i < 10000; ++i) apps[i].name = "THIS ENTRY MUST NOT APPEAR ON RECENT";
    assert(before == pixels(s, apps)); pass("Recent rendering ignores all entries after its five bounded slots"); }
  { auto apps = sample(10000); State s(apps.size(), 5); s.key(Key::Right); s.touch_down(296, 140); s.touch_move(296, 510); s.touch_up(); row(s, 9999);
    const auto before = pixels(s, apps); apps[9999].name = "FINAL APP VISIBLE"; assert(before != pixels(s, apps));
    pass("the real renderer displays the final viewport, not an abbreviated app list"); }
  { auto apps = sample(1); apps[0].description = std::string(6000, 'W'); const int height = details_height(apps[0]);
    assert(height > 2000); State s(1, 1); s.key(Key::Info); s.set_detail_height(height); auto before = pixels(s, apps);
    s.key(Key::Right); assert(before != pixels(s, apps)); for (int i = 0; i < 1000; ++i) s.key(Key::Down);
    assert(s.detail_scroll == height - Layout::detail_body.h); pass("long app descriptions scroll to their measured end without a combined metadata copy"); }
  { auto apps = sample(1); apps[0].name = "Gr\xc3\xb6\xc3\x9f" "en \xc3\xa4\xc3\xb6\xc3\xbc / \xf0\x9f\x98\x80";
    apps[0].description = std::string(4000, '\n') + std::string(3000, 'W') + "\xc3";
    constexpr int guard = 37; std::vector<std::uint16_t> out(73 * 41 + 2 * guard, 0x1234); State s(1, 1);
    for (int detail = 0; detail < 2; ++detail) { s.details = detail; render({out.data() + guard, 73, 41}, s, apps);
      assert(std::all_of(out.begin(), out.begin() + guard, [](auto p) { return p == 0x1234; }));
      assert(std::all_of(out.end() - guard, out.end(), [](auto p) { return p == 0x1234; })); }
    pass("clipped canvases, malformed UTF-8 and long metadata remain inside framebuffer bounds"); }
  { std::mt19937 rng(20261123); State s(1000, 1000); s.set_detail_height(4000);
    for (int i = 0; i < 50000; ++i) {
      switch (rng() % 5) {
      case 0: s.touch_down(static_cast<int>(rng() % 380) - 30, static_cast<int>(rng() % 590) - 30); break;
      case 1: s.touch_move(static_cast<int>(rng() % 380) - 30, static_cast<int>(rng() % 590) - 30); break;
      case 2: s.touch_up(); break;
      default: s.key(static_cast<Key>(rng() % 7), rng() % 2); break;
      }
      assert(s.selected >= 0 && s.selected < s.view_count());
      assert(s.first >= 0 && s.first <= std::max(0, s.view_count() - 6)); s.action = Action::None;
    }
    pass("50000 mixed input events preserve bounds across Recent, All apps and Info"); }
  if (argc == 2) {
    const std::string dir = argv[1]; auto apps = sample(1000); State s(apps.size(), 5);
    ppm(dir + "/compact-recent.ppm", s, apps); s.key(Key::Right); ppm(dir + "/compact-all.ppm", s, apps);
    s.touch_down(296, 140); s.touch_move(296, 510); s.touch_up(); row(s, 999); ppm(dir + "/compact-end.ppm", s, apps);
    s.key(Key::Left); s.touch_down(60, 467); ppm(dir + "/compact-pressed.ppm", s, apps); s.cancel_touch();
    s.key(Key::Info); ppm(dir + "/compact-info.ppm", s, apps);
    State empty(0); ppm(dir + "/compact-empty-recent.ppm", empty, {}); empty.key(Key::Right); ppm(dir + "/compact-empty-all.ppm", empty, {});
  }
  std::cout << passed << "/" << passed << " Compact UI checks passed\n";
}
