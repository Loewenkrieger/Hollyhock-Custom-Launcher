// SPDX-License-Identifier: GPL-3.0-or-later
#include "recent.hpp"
#include "variant_fixture.hpp"
#include <sdk/os/mcs.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <random>
#include <vector>

namespace {
std::string stored;
std::uint8_t stored_type = VARTYPE_STR;
std::uint32_t reported_size = 0;
MCS_Error read_status = MCS_NO_FOLDER, create_status = MCS_OK, write_status = MCS_OK;
int creates = 0, writes = 0, passed = 0;
void pass(const char *message) { ++passed; std::cout << "PASS " << message << '\n'; }
void check_names(const char *folder, const char *variable) {
  assert(std::strcmp(folder, "hhlaunch") == 0 && std::strcmp(variable, target::history_variable) == 0);
}
MCS_Error get(const char *folder, const char *variable, std::uint8_t *type,
              char **name, void **data, std::uint32_t *size) {
  check_names(folder, variable);
  *type = stored_type; *name = nullptr; *data = stored.empty() ? nullptr : stored.data();
  *size = reported_size;
  return read_status;
}
MCS_Error create(const char *folder, std::uint8_t *index) {
  assert(std::strcmp(folder, "hhlaunch") == 0); *index = 7; ++creates; return create_status;
}
MCS_Error set(const char *folder, const char *variable, std::uint8_t type,
              std::uint32_t size, void *data) {
  check_names(folder, variable); ++writes;
  assert(type == VARTYPE_STR && size <= 512 && size > 0);
  assert(static_cast<char *>(data)[size - 1] == '\0');
  if (write_status == MCS_OK) {
    stored.assign(static_cast<char *>(data), size); stored_type = type;
    reported_size = size; read_status = MCS_OK;
  }
  return write_status;
}
void reset() {
  stored.clear(); stored_type = VARTYPE_STR; reported_size = 0;
  read_status = MCS_NO_FOLDER; create_status = write_status = MCS_OK;
  creates = writes = 0;
}
void payload(std::string value) {
  stored = std::move(value); stored.push_back('\0');
  reported_size = static_cast<std::uint32_t>(stored.size()); read_status = MCS_OK;
}
struct FakeApp final : FileBasedExecutable<"" APP_EXT ""> {
  using FileBasedExecutable::FileBasedExecutable;
  void load() override {}
  void unload() override {}
  int execute() override { return 0; }
};
auto apps(std::initializer_list<const char *> names) {
  std::forward_list<std::unique_ptr<Executable>> result;
  auto tail = result.before_begin();
  for (const auto *name : names) {
    const auto path = std::string("\\fls0\\") + name + "" APP_EXT "";
    tail = result.emplace_after(tail, std::make_unique<FakeApp>(path.c_str()));
  }
  return result;
}
auto paths(const std::forward_list<std::unique_ptr<Executable>> &list) {
  std::vector<std::string> result;
  for (const auto &app : list) result.emplace_back(app->pathView());
  return result;
}
}

extern "C" {
MCS_Error (*MCS_GetVariableU)(const char *, const char *, std::uint8_t *, char **,
                            void **, std::uint32_t *) = get;
MCS_Error (*MCS_CreateFolder)(const char *, std::uint8_t *) = create;
MCS_Error (*MCS_SetVariableU)(const char *, const char *, std::uint8_t,
                            std::uint32_t, void *) = set;
}

int main() {
  reset(); assert(recent::load().size == 0 && creates == 0 && writes == 0);
  pass("missing history opens normally without creating or writing MCS data");

  { recent::History history; recent::remember("\\fls0\\one" APP_EXT "", history);
    auto loaded = recent::load();
    assert(creates == 1 && writes == 1 && loaded.size == 1 && loaded.paths[0] == "\\fls0\\one" APP_EXT "");
    pass("history round-trips through the actual SDK byte-type declarations"); }

  { reset(); recent::History history;
    for (int i = 0; i < 7; ++i) recent::remember("\\fls0\\app" + std::to_string(i) + "" APP_EXT "", history);
    recent::remember("\\fls0\\app4" APP_EXT "", history);
    const auto loaded = recent::load();
    assert(loaded.size == 5 && loaded.paths[0] == "\\fls0\\app4" APP_EXT "" &&
           loaded.paths[1] == "\\fls0\\app6" APP_EXT "" && loaded.paths[4] == "\\fls0\\app2" APP_EXT "");
    pass("five-entry MRU moves a repeated app to the front without duplicates"); }

  { reset(); payload("" HISTORY "\\fls0\\c" APP_EXT "\n\\fls0\\missing" APP_EXT "\n\\fls0\\b" APP_EXT "\n");
    auto history = recent::load(); auto list = apps({"a", "b", "c", "d"});
    assert(recent::prioritize(list, history) == 2 && history.size == 2);
    assert((paths(list) == std::vector<std::string>{"\\fls0\\c" APP_EXT "", "\\fls0\\b" APP_EXT "", "\\fls0\\a" APP_EXT "", "\\fls0\\d" APP_EXT ""}));
    assert(creates == 0 && writes == 0);
    pass("existing recent apps lead the list; missing apps are dropped; all other apps retain their order"); }

  { reset(); payload("" HISTORY "\\fls0\\one" APP_EXT "\n\\fls0\\one" APP_EXT "\nwrong" APP_EXT "\n\\fls0\\sub\\two" APP_EXT "\n\\fls0\\old" OTHER_EXT "\n\\fls0\\two" APP_EXT "\n");
    auto history = recent::load(); assert(history.size == 2 && history.paths[1] == "\\fls0\\two" APP_EXT "");
    pass("invalid paths and duplicate stored entries are skipped"); }

  { reset(); payload("WRONG\n\\fls0\\one" APP_EXT "\n"); assert(recent::load().size == 0);
    payload("" HISTORY "\\fls0\\one" APP_EXT ""); assert(recent::load().size == 0);
    stored.pop_back(); reported_size = static_cast<std::uint32_t>(stored.size());
    assert(recent::load().size == 0 && writes == 0);
    pass("unknown, incomplete and unterminated records fall back without writes"); }

  { reset(); payload("" HISTORY "\\fls0\\one" APP_EXT "\n"); stored_type = VARTYPE_PRGM;
    assert(recent::load().size == 0); stored_type = VARTYPE_STR; reported_size = 513;
    assert(recent::load().size == 0); reported_size = 0; assert(recent::load().size == 0);
    stored.clear(); reported_size = 20; assert(recent::load().size == 0);
    pass("wrong variable types, oversized records and null data are ignored before parsing"); }

  { reset(); payload("" HISTORY "\\fls0\\one" APP_EXT "\n"); read_status = MCS_NO_VARIABLE;
    assert(recent::load().size == 0); MCS_GetVariableU = nullptr;
    assert(recent::load().size == 0); MCS_GetVariableU = get;
    pass("unavailable or failing MCS reads do not affect normal app selection"); }

  { reset(); recent::History history; create_status = static_cast<MCS_Error>(1);
    recent::remember("\\fls0\\one" APP_EXT "", history); assert(writes == 0 && history.size == 0);
    create_status = MCS_OK; write_status = static_cast<MCS_Error>(1);
    recent::remember("\\fls0\\one" APP_EXT "", history); assert(writes == 1 && history.size == 0);
    pass("folder and write errors are non-fatal and leave the in-memory record intact"); }

  { reset(); recent::History history; create_status = MCS_FOLDER_EXISTS;
    recent::remember("\\fls0\\one" APP_EXT "", history); assert(writes == 1 && recent::load().size == 1);
    pass("an existing launcher folder is reused"); }

  { reset(); recent::History history;
    recent::remember("\\fls0\\TETRIS" APP_UPPER_EXT "", history);
    assert(recent::load().paths[0] == "\\fls0\\TETRIS" APP_UPPER_EXT "");
    pass("uppercase file extensions retain their discovered filename in history"); }

  { reset(); recent::History history;
    for (const auto *bad : {"", "one" APP_EXT "", "\\fls0\\run.bin", "\\fls0\\bad\n" APP_EXT "", "\\fls0\\sub/app" APP_EXT ""})
      recent::remember(bad, history);
    assert(creates == 0 && writes == 0);
    MCS_CreateFolder = nullptr; recent::remember("\\fls0\\one" APP_EXT "", history); MCS_CreateFolder = create;
    MCS_SetVariableU = nullptr; recent::remember("\\fls0\\one" APP_EXT "", history); MCS_SetVariableU = set;
    assert(creates == 0 && writes == 0);
    pass("invalid launch paths and unavailable storage never trigger a write"); }

  { reset(); recent::History history;
    for (int i = 0; i < 5; ++i)
      recent::remember("\\fls0\\" + std::string(88, static_cast<char>('a' + i)) + std::to_string(i) + "" APP_EXT "", history);
    assert(history.size == 5 && stored.size() <= 512 && recent::load().size == 5);
    pass("five maximum-length discovered paths fit in a bounded record"); }

  { reset(); std::mt19937 random(20261029);
    for (int i = 0; i < 10000; ++i) {
      stored.assign(random() % 600, '\0');
      for (auto &c : stored) c = static_cast<char>(random());
      if (i % 2 == 0 && stored.size() >= 8) stored.replace(0, 8, "" HISTORY "");
      reported_size = static_cast<std::uint32_t>(stored.size()); read_status = MCS_OK;
      assert(recent::load().size <= recent::capacity);
    }
    assert(writes == 0 && creates == 0);
    pass("10000 malformed records remain bounded and never write MCS"); }

  reset();
  payload(std::string(target::hh2 ? "HH3REC1\n" : "HH2REC1\n") + "\\fls0\\other" APP_EXT "\n");
  assert(recent::load().size == 0 && creates == 0 && writes == 0);
  assert(std::strcmp(target::history_variable, target::hh2 ? "recent2" : "recent") == 0);
  pass("the other variant's record is ignored and history storage stays separate");
#if LAUNCHER_HH2
  reset();
  { recent::History history;
    recent::remember("\\fls0\\game.bin", history);
    assert(recent::load().size == 1 && recent::load().paths[0] == "\\fls0\\game.bin");
    const auto before = writes;
    recent::remember("\\fls0\\RUN.BIN", history);
    recent::remember("\\fls0\\run-hh3-os7002.bin", history);
    recent::remember("\\fls0\\other.hh3", history);
    assert(writes == before);
  }
  pass("HH2 BIN history is retained while launchers and HH3 paths are excluded");
#endif
  std::cout << passed << '/' << passed << " history checks passed\n";
}
