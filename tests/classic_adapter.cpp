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
  { auto list = apps(); reset({touch(TOUCH_DOWN, 45, 55), touch(TOUCH_UP, 0, 0),
      touch(TOUCH_DOWN, 50, 74 + 28 + 10), touch(TOUCH_UP, -500, -500),
      touch(TOUCH_DOWN, 50, 95), touch(TOUCH_UP, 0, 0)});
    auto chosen = do_gui(list); assert(chosen && static_cast<FakeApp *>(chosen.get())->id == 1 && position == 6);
    std::cout << "PASS real adapter: short dropdown/Run taps with invalid lift coordinates choose second app entry\n"; }
  { auto list = apps(); reset({key(KEYCODE_EXE, KEY_RELEASED), key(KEYCODE_DOWN), key(KEYCODE_EXE)});
    auto chosen = do_gui(list); assert(chosen && static_cast<FakeApp *>(chosen.get())->id == 1 && position == 3);
    std::cout << "PASS real adapter: released EXE ignored; Down + EXE selects intended app\n"; }
  { auto list = apps(); Input_Event idle{}; idle.type = EVENT_TIMER;
    reset({idle, idle, touch(TOUCH_DOWN, 50, 95), touch(TOUCH_UP, 0, 0)});
    auto chosen = do_gui(list); assert(chosen && static_cast<FakeApp *>(chosen.get())->id == 0 && refreshes == 2);
    std::cout << "PASS real adapter: idle events do not redraw or swallow the following tap\n"; }
  { auto list = apps(); reset({touch(TOUCH_DOWN, 50, 95), touch(TOUCH_HOLD_DRAG, 180, 200),
      touch(TOUCH_UP, 50, 95), key(KEYCODE_POWER_CLEAR)});
    assert(!do_gui(list)); assert(position == 4);
    std::cout << "PASS real adapter: drag cancellation followed by physical Clear exits\n"; }
  { auto list = apps(1000); reset({key(KEYCODE_POWER_CLEAR)});
    assert(!do_gui(list));
    assert(name_reads == 1000 && detail_reads[0] == 3);
    assert(std::all_of(detail_reads.begin() + 1, detail_reads.end(), [](int n) { return n == 0; }));
    std::cout << "PASS real adapter: 1000 app names remain available; startup copies details for only one app\n"; }
  { auto list = apps(); reset({key(KEYCODE_DOWN), key(KEYCODE_UP), key(KEYCODE_DOWN), key(KEYCODE_EXE)});
    const auto chosen = do_gui(list);
    assert(chosen && static_cast<FakeApp *>(chosen.get())->id == 1);
    assert(detail_reads[0] == 3 && detail_reads[1] == 3);
    std::cout << "PASS real adapter: revisiting a selection reuses its cached detail strings\n"; }
  { auto list = apps(); list.reverse(); reset({key(KEYCODE_EXE)});
    const auto chosen = do_gui(list, 1);
    assert(chosen && static_cast<FakeApp *>(chosen.get())->id == 1 && position == 1);
    std::cout << "PASS real adapter: EXE launches the preselected recent entry with correct executable mapping\n"; }
  std::cout << "7/7 adapter checks passed\n";
}
