// SPDX-License-Identifier: GPL-3.0-or-later
// Run the real main() and history implementation. UI, discovery and app
// execution are replaced; no calculator memory or SH instructions are used.
#include "gui.hpp"
#include "variant_fixture.hpp"
#include <sdk/os/mcs.h>
#include <sdk/os/gui.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <stdexcept>

int launcher_main();
namespace {
bool close_menu = false, fail_load = false, fail_execute = false, fail_write = false;
bool loaded = false;
int executions = 0, saves = 0, unloads = 0, backups = 0, restores = 0, errors = 0;
std::size_t visible_recents = 0;
std::string record, shown_path;
struct FakeApp final : FileBasedExecutable<"" APP_EXT ""> {
  using FileBasedExecutable::FileBasedExecutable;
  void load() override {
    if (fail_load) throw std::runtime_error("load failed");
    loaded = true;
  }
  void unload() override { ++unloads; }
  int execute() override {
    assert(loaded && saves == 1); ++executions;
    if (fail_execute) throw std::runtime_error("execute failed");
    return 0;
  }
};
MCS_Error get(const char *, const char *, std::uint8_t *type, char **name,
              void **data, std::uint32_t *size) {
  *type = VARTYPE_STR; *name = nullptr; *data = record.data();
  *size = static_cast<std::uint32_t>(record.size()); return MCS_OK;
}
MCS_Error create(const char *, std::uint8_t *) { return MCS_FOLDER_EXISTS; }
MCS_Error set(const char *, const char *, std::uint8_t, std::uint32_t size, void *data) {
  assert(loaded && executions == 0); ++saves;
  if (fail_write) return static_cast<MCS_Error>(1);
  record.assign(static_cast<char *>(data), size); return MCS_OK;
}
void *message(int, const char *, const char *, const char *, int, bool) {
  ++errors; return nullptr;
}
void reset() {
  close_menu = fail_load = fail_execute = fail_write = loaded = false;
  executions = saves = unloads = backups = restores = errors = 0;
  record = "" HISTORY "\\fls0\\b" APP_EXT "\n"; record.push_back('\0');
  shown_path.clear(); visible_recents = 0;
}
}
extern "C" {
#if LAUNCHER_HH2
extern const std::uint8_t addresses[] = "2017.0512.1515";
#else
extern const std::uint8_t addresses[] = "2023.0209.1500";
#endif
extern const std::size_t addresses_size = sizeof(addresses);
bool relink_sdk(const std::uint8_t *, std::size_t) { assert(false); return false; }
bool ___relink(const std::uint8_t *, std::size_t) { assert(false); return false; }
std::FILE *__wrap_fopen(const char *, const char *) { return nullptr; }
void calcInit() { ++backups; }
void calcExit() { ++restores; }
MCS_Error (*MCS_GetVariableU)(const char *, const char *, std::uint8_t *, char **,
                            void **, std::uint32_t *) = get;
MCS_Error (*MCS_CreateFolder)(const char *, std::uint8_t *) = create;
MCS_Error (*MCS_SetVariableU)(const char *, const char *, std::uint8_t,
                            std::uint32_t, void *) = set;
void *(*GUI_DisplayMessageBox_Internal)(int, const char *, const char *, const char *, int, bool) = message;
}
std::forward_list<std::unique_ptr<Executable>> discover_apps() {
  std::forward_list<std::unique_ptr<Executable>> list;
  list.emplace_front(std::make_unique<FakeApp>("\\fls0\\b" APP_EXT ""));
  list.emplace_front(std::make_unique<FakeApp>("\\fls0\\a" APP_EXT "")); return list;
}
std::unique_ptr<Executable> do_gui(std::forward_list<std::unique_ptr<Executable>> &list,
                                  std::size_t recent_count) {
  visible_recents = recent_count; shown_path = list.front()->pathView();
  if (close_menu) return nullptr;
  return std::move(list.front());
}
int main() {
  reset(); close_menu = true;
  assert(launcher_main() == 0 && saves == 0 && executions == 0 && !loaded);
  assert(visible_recents == 1 && shown_path == "\\fls0\\b" APP_EXT "");
  std::cout << "PASS main: history is restored before selection; closing writes nothing\n";
  reset();
  assert(launcher_main() == 0 && saves == 1 && executions == 1 && unloads == 1 && errors == 0);
  assert(restores == 1 && backups == 1);
  std::cout << "PASS main: successful load saves history once before app execution\n";
  reset(); fail_load = true;
  assert(launcher_main() == 1 && saves == 0 && executions == 0 && unloads == 1 && errors == 1);
  assert(restores == 1 && backups == 1);
  std::cout << "PASS main: failed loading leaves history untouched and restores the display\n";
  reset(); fail_write = true;
  assert(launcher_main() == 0 && saves == 1 && executions == 1 && errors == 0);
  std::cout << "PASS main: an MCS write failure cannot prevent app execution\n";
  reset(); fail_execute = true;
  assert(launcher_main() == 1 && saves == 1 && executions == 1 && unloads == 1 && errors == 1);
  assert(restores == 1 && backups == 1);
  std::cout << "PASS main: app errors retain cleanup and the explicit error dialog\n";
  std::cout << "5/5 launch integration checks passed\n";
}
