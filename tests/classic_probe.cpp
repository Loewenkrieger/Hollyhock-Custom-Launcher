#include "variant_fixture.hpp"
// SPDX-License-Identifier: GPL-3.0-or-later
#include "classic/ui.hpp"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace classic;
static int passed = 0;
static void pass(const char *name) { ++passed; std::cout << "PASS " << name << '\n'; }
static void tap(State &s, int x, int y) { s.touch_down(x, y); s.touch_up(); }
static std::vector<AppInfo> sample(int count) {
  std::vector<AppInfo> apps;
  for (int i = 0; i < count; ++i)
    apps.push_back({i == 0 ? "Tetris" : "Application " + std::to_string(i + 1),
                    i == 0 ? "\\fls0\\tetris" APP_EXT "" : "\\fls0\\app" APP_EXT "", "1.0.0", "De_Coder",
                    "A simple implementation of the game Tetris."});
  return apps;
}
static void ppm(const std::string &path, const State &s, const std::vector<AppInfo> &apps) {
  std::vector<std::uint16_t> pixels(320 * 528);
  render({pixels.data(), 320, 528}, s, apps);
  std::ofstream file(path, std::ios::binary);
  file << "P6\n320 528\n255\n";
  for (auto pixel : pixels) {
    const char rgb[] = {static_cast<char>(((pixel >> 11) & 31) * 255 / 31),
                        static_cast<char>(((pixel >> 5) & 63) * 255 / 63),
                        static_cast<char>((pixel & 31) * 255 / 31)};
    file.write(rgb, 3);
  }
}

int main(int argc, char **argv) {
  { State s(0); tap(s, 50, 95); s.key(Key::Enter); assert(s.selected == -1 && s.action == Action::None && !s.open); tap(s, 245, 95); assert(s.action == Action::Close); pass("empty list cannot start; Close remains active"); }
  { State s(5); assert(s.selected == 0); tap(s, 50, 55); assert(s.open); tap(s, 80, 74 + 2 * 28 + 10); assert(!s.open && s.selected == 2); pass("dropdown selects tapped row"); }
  { State s(2); tap(s, 50, 55); tap(s, 50, 55); assert(!s.open); pass("selector toggles dropdown"); }
  { State s(4); tap(s, 50, 55); tap(s, 20, 470); assert(!s.open && s.action == Action::None); pass("outside tap dismisses without click-through"); }
  { State s(1); s.touch_down(50, 95); assert(s.action == Action::None); s.touch_up(); assert(s.action == Action::Run); assert(!s.touch_up()); pass("short DOWN-UP tap starts exactly once; no duration threshold"); }
  { State s(1); s.touch_down(50, 95); s.touch_move(52, 96); s.touch_up(); assert(s.action == Action::Run); pass("small stylus movement is tolerated"); }
  { State s(1); s.touch_down(50, 95); assert(s.visually_pressed(Target::Run)); s.touch_move(190, 200); assert(!s.visually_pressed(Target::Run)); s.touch_up(); assert(s.action == Action::None); pass("dragging away cancels button activation and pressed appearance"); }
  { State s(2); s.touch_down(50, 95); s.touch_move(-1, -1); s.touch_up(); assert(s.action == Action::Run); pass("invalid lift/movement coordinate cannot invalidate captured tap"); }
  { State s(2); s.key(Key::Down); assert(s.selected == 1); s.key(Key::Enter); assert(s.action == Action::Run); pass("arrow selection and EXE launch"); }
  { State s(2); s.key(Key::Up); assert(s.selected == 0); s.key(Key::Down); s.key(Key::Down, true); assert(s.selected == 1); pass("selection remains in bounds on key repeat"); }
  { State s(3); tap(s, 50, 55); s.key(Key::Down); s.key(Key::Enter); assert(!s.open && s.selected == 1 && s.action == Action::None); s.key(Key::Enter, true); assert(s.action == Action::None); s.key(Key::Enter); assert(s.action == Action::Run); pass("dropdown confirmation does not double-launch on held EXE"); }
  { State s(1); s.key(Key::Right); assert(s.focus == Target::Run); s.key(Key::Right); assert(s.focus == Target::Close); s.key(Key::Enter); assert(s.action == Action::Close); pass("keyboard focus reaches Close"); }
  { State s(1); tap(s, 50, 55); s.key(Key::Escape); assert(!s.open && s.action == Action::None); s.key(Key::Escape, true); assert(s.action == Action::None); s.key(Key::Escape); assert(s.action == Action::Close); pass("Clear closes dropdown first and then launcher"); }
  { State s(1); tap(s, 300, 18); assert(s.action == Action::Close); pass("title X closes launcher"); }
  { State s(50); tap(s, 50, 55); s.touch_down(50, 150); s.touch_move(50, 60); s.touch_up(); assert(s.first == 3 && s.open && s.selected == 0); pass("dropdown drag scrolls without selecting an app"); }
  { State s(50); tap(s, 50, 55); s.touch_down(50, 150); assert(s.visually_pressed(Target::Row)); assert(s.touch_move(50, 137)); assert(s.first == 0 && !s.visually_pressed(Target::Row)); s.touch_up(); assert(s.open && s.selected == 0); pass("starting a drag clears row feedback before the first scroll step"); }
  { State s(5); tap(s, 50, 55); s.touch_down(50, 150); assert(s.touch_move(310, 150)); assert(!s.visually_pressed(Target::Row)); s.touch_up(); assert(s.open && s.selected == 0); pass("leaving a row horizontally redraws its cancelled press"); }
  { State s(50); tap(s, 50, 55); s.key(Key::Right); assert(s.selected == 10 && s.first == 1); for (int i = 0; i < 10; ++i) s.key(Key::Right); assert(s.selected == 49 && s.first == 40); pass("keyboard paging reaches final apps"); }
  { State s(50); tap(s, 50, 55); auto r = s.popup(); tap(s, 290, r.y + r.h - 10); assert(s.first == 1 && s.open); tap(s, 290, r.y + 10); assert(s.first == 0 && s.open); pass("scroll arrows advance one row"); }
  { State s(3); for (int i = 0; i < 1000; ++i) { tap(s, 50, 55); s.touch_down(50, 90); s.touch_up(); assert(!s.open); } assert(s.action == Action::None); pass("1000 queued tap pairs preserve all transitions"); }
  { constexpr int guard = 37; std::vector<std::uint16_t> pixels(320 * 528 + 2 * guard, 0x1234); auto apps = sample(80); State s(apps.size()); tap(s, 50, 55); s.key(Key::Right); render({pixels.data() + guard, 320, 528}, s, apps); assert(std::all_of(pixels.begin(), pixels.begin() + guard, [](auto p) { return p == 0x1234; })); assert(std::all_of(pixels.end() - guard, pixels.end(), [](auto p) { return p == 0x1234; })); pass("renderer respects framebuffer boundaries with open dropdown"); }
  { auto apps = sample(1); apps[0].name = "Gr\xc3\xb6\xc3\x9f" "en \xc3\xa4\xc3\xb6\xc3\xbc / \xf0\x9f\x98\x80"; apps[0].description = std::string(3000, 'W') + "\n" + std::string(4000, '\n') + "\xc3"; std::vector<std::uint16_t> pixels(73 * 41 + 74, 0x1234); State s(1); render({pixels.data() + 37, 73, 41}, s, apps); assert(std::all_of(pixels.begin(), pixels.begin() + 37, [](auto p) { return p == 0x1234; })); assert(std::all_of(pixels.end() - 37, pixels.end(), [](auto p) { return p == 0x1234; })); pass("long text, UTF-8, malformed text and clipped canvas remain bounded"); }
  { std::mt19937 rng(20261028); State s(100); for (int i = 0; i < 25000; ++i) { switch (rng() % 5) { case 0: s.touch_down(static_cast<int>(rng() % 380) - 30, static_cast<int>(rng() % 590) - 30); break; case 1: s.touch_move(static_cast<int>(rng() % 380) - 30, static_cast<int>(rng() % 590) - 30); break; case 2: s.touch_up(); break; default: s.key(static_cast<Key>(rng() % 6), rng() % 2); break; } assert(s.selected >= 0 && s.selected < 100); assert(s.first >= 0 && s.first <= 90); s.action = Action::None; } pass("25000 mixed input events preserve selection and scroll bounds"); }
  if (argc == 2) {
    const std::string directory = argv[1];
    auto apps = sample(15); State normal(apps.size()); ppm(directory + "/classic-closed.ppm", normal, apps);
    tap(normal, 50, 55); ppm(directory + "/classic-open.ppm", normal, apps);
    for (int i = 0; i < 3; ++i) apps[i].recent = true;
    ppm(directory + "/classic-recent.ppm", normal, apps);
    State pressed(apps.size()); pressed.touch_down(50, 95); ppm(directory + "/classic-pressed.ppm", pressed, apps);
    State empty(0); ppm(directory + "/classic-empty.ppm", empty, {});
  }
  std::cout << passed << "/" << passed << " checks passed\n";
}
