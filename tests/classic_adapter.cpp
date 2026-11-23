#include "variant_fixture.hpp"
// SPDX-License-Identifier: GPL-3.0-or-later
// Compile the real gui.cpp against the original SDK declarations. Only the
// three display functions and GetInput are replaced with host fakes.
#include "gui.hpp"
#include <sdk/os/input.h>
#include <cassert>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
std::vector<Input_Event> events;
std::size_t position;
int refreshes;
int name_reads;
std::array<int, 1000> detail_reads{};
std::uint16_t pixels[320 * 528];
void size(unsigned *w, unsigned *h) { *w = 320; *h = 528; }
std::uint16_t *framebuffer() { return pixels; }
void refresh() { ++refreshes; }
int input(Input_Event *out, std::uint32_t wait, std::uint32_t mode) {
  assert(wait == 0xffffffffu && mode == 0x10);
  assert(out->zero == 0 && static_cast<unsigned>(out->type) == 0);
  if (position == events.size()) throw std::runtime_error("UI consumed all events without returning");
  *out = events[position++];
  return 0;
}
Input_Event touch(Input_TouchEventType type, int x, int y) {
  Input_Event e{}; e.type = EVENT_TOUCH; e.data.touch_single.direction = type;
  e.data.touch_single.p1_x = x; e.data.touch_single.p1_y = y; return e;
}
Input_Event key(Input_Keycode code, Input_KeyEventType direction = KEY_PRESSED) {
  Input_Event e{}; e.type = EVENT_KEY; e.data.key.keyCode = code; e.data.key.direction = direction; return e;
}
std::unique_ptr<char[]> copy(const char *s) {
  auto out = std::make_unique<char[]>(std::strlen(s) + 1); std::strcpy(out.get(), s); return out;
}
struct FakeApp final : Executable {
  int id;
  std::string file_path;
  explicit FakeApp(int number) : id(number), file_path(number == 0 ? "\\fls0\\one" APP_EXT "" : number == 1 ? "\\fls0\\two" APP_EXT "" : "\\fls0\\app" + std::to_string(number) + "" APP_EXT "") {}
  void load() override { assert(false); }
  void unload() override { assert(false); }
  int execute() override { assert(false); return 0; }
  std::string_view pathView() const override { return file_path; }
  std::unique_ptr<char[]> getName() override { ++name_reads; return copy(id == 0 ? "One" : id == 1 ? "Two" : file_path.c_str()); }
  std::unique_ptr<char[]> getVersion() override { ++detail_reads[id]; return copy("1.0"); }
  std::unique_ptr<char[]> getAuthor() override { ++detail_reads[id]; return copy("Author"); }
  std::unique_ptr<char[]> getDescription() override { ++detail_reads[id]; return copy("Description"); }
};
std::forward_list<std::unique_ptr<Executable>> apps(int count = 2) {
  std::forward_list<std::unique_ptr<Executable>> out;
  auto tail = out.before_begin();
  for (int i = 0; i < count; ++i) tail = out.emplace_after(tail, std::make_unique<FakeApp>(i));
  return out;
}
void reset(std::vector<Input_Event> queue) {
  events = std::move(queue); position = 0; refreshes = name_reads = 0; detail_reads.fill(0);
}
}

extern "C" {
void (*LCD_GetSize)(unsigned *, unsigned *) = size;
std::uint16_t *(*_FP_LCD_GetVRAMAddress)() = framebuffer;
void (*LCD_Refresh)() = refresh;
int (*GetInput)(Input_Event *, std::uint32_t, std::uint32_t) = input;
}

int main() {
  { auto list = apps(); reset({touch(TOUCH_DOWN, 220, 98), touch(TOUCH_UP, 0, 0),
      touch(TOUCH_DOWN, 50, 188), touch(TOUCH_UP, -500, -500),
      touch(TOUCH_DOWN, 50, 466), touch(TOUCH_UP, 0, 0)});
    auto chosen = do_gui(list); assert(chosen && static_cast<FakeApp *>(chosen.get())->id == 1 && position == 6);
    std::cout << "PASS real adapter: short All apps/row/Run taps retain the correct executable mapping\n"; }
  { auto list = apps(); reset({key(KEYCODE_EXE, KEY_RELEASED), key(KEYCODE_RIGHT), key(KEYCODE_DOWN), key(KEYCODE_EXE)});
    auto chosen = do_gui(list); assert(chosen && static_cast<FakeApp *>(chosen.get())->id == 1 && position == 4);
    std::cout << "PASS real adapter: released EXE ignored; Right, Down and EXE select the intended app\n"; }
  { auto list = apps(); Input_Event idle{}; idle.type = EVENT_TIMER;
    reset({idle, idle, touch(TOUCH_DOWN, 50, 466), touch(TOUCH_UP, 0, 0)});
    auto chosen = do_gui(list, 2); assert(chosen && static_cast<FakeApp *>(chosen.get())->id == 0 && refreshes == 2);
    std::cout << "PASS real adapter: idle events do not redraw or swallow the following tap\n"; }
  { auto list = apps(); reset({touch(TOUCH_DOWN, 50, 466), touch(TOUCH_HOLD_DRAG, 180, 200),
      touch(TOUCH_UP, 50, 466), key(KEYCODE_POWER_CLEAR)});
    assert(!do_gui(list, 2)); assert(position == 4);
    std::cout << "PASS real adapter: drag cancellation followed by physical Clear exits\n"; }
  { auto list = apps(1000); reset({key(KEYCODE_POWER_CLEAR)});
    assert(!do_gui(list));
    assert(name_reads == 1000);
    assert(std::all_of(detail_reads.begin(), detail_reads.end(), [](int n) { return n == 0; }));
    std::cout << "PASS real adapter: empty Recent keeps all 1000 names available without copying app details\n"; }
  { auto list = apps(); reset({key(KEYCODE_RIGHT), key(KEYCODE_DOWN), key(KEYCODE_UP), key(KEYCODE_DOWN), key(KEYCODE_EXE)});
    const auto chosen = do_gui(list);
    assert(chosen && static_cast<FakeApp *>(chosen.get())->id == 1);
    assert(detail_reads[0] == 3 && detail_reads[1] == 3);
    std::cout << "PASS real adapter: revisiting a selection reuses its cached detail strings\n"; }
  { auto list = apps(); list.reverse(); reset({key(KEYCODE_EXE)});
    const auto chosen = do_gui(list, 1);
    assert(chosen && static_cast<FakeApp *>(chosen.get())->id == 1 && position == 1);
    std::cout << "PASS real adapter: EXE launches the preselected recent entry with correct executable mapping\n"; }
  { auto list = apps(1000); reset({key(KEYCODE_RIGHT), touch(TOUCH_DOWN, 296, 140),
      touch(TOUCH_HOLD_DRAG, 296, 510), touch(TOUCH_UP, -1, -1),
      touch(TOUCH_DOWN, 60, 340), touch(TOUCH_UP, -1, -1),
      key(KEYCODE_LEFT), key(KEYCODE_RIGHT), key(KEYCODE_EXE)});
    auto chosen = do_gui(list, 1000); assert(chosen && static_cast<FakeApp *>(chosen.get())->id == 999);
    assert(detail_reads[0] == 3 && detail_reads[999] == 3);
    assert(std::all_of(detail_reads.begin() + 1, detail_reads.end() - 1, [](int n) { return n == 0; }));
    std::cout << "PASS real adapter: thumb scrolling reaches app 1000, restores page state and reads only selected metadata\n"; }
  { auto list = apps(); reset({key(KEYCODE_0), key(KEYCODE_DOWN), key(KEYCODE_POWER_CLEAR), key(KEYCODE_EXE)});
    auto chosen = do_gui(list, 1); assert(chosen && static_cast<FakeApp *>(chosen.get())->id == 0);
    std::cout << "PASS real adapter: physical 0 opens Info, Clear returns and EXE starts the selected app\n"; }
  { auto list = apps(); reset({key(KEYCODE_EXE), key(KEYCODE_0), key(KEYCODE_RIGHT), key(KEYCODE_EXE)});
    auto chosen = do_gui(list); assert(chosen && static_cast<FakeApp *>(chosen.get())->id == 0 && position == 4);
    std::cout << "PASS real adapter: empty Recent ignores Run/Info until All apps is selected\n"; }
  { auto list = apps(1000); reset({key(KEYCODE_DOWN), key(KEYCODE_DOWN), key(KEYCODE_DOWN), key(KEYCODE_DOWN),
      key(KEYCODE_DOWN), key(KEYCODE_DOWN), key(KEYCODE_EXE)});
    auto chosen = do_gui(list, 1000); assert(chosen && static_cast<FakeApp *>(chosen.get())->id == 4);
    assert(std::all_of(detail_reads.begin() + 5, detail_reads.end(), [](int n) { return n == 0; }));
    std::cout << "PASS real adapter: even an oversized recent count exposes at most five apps\n"; }
  std::cout << "11/11 adapter checks passed\n";
}
