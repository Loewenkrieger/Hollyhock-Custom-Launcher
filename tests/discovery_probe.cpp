// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise production app discovery against the SDK declarations. Only file
// enumeration and ELF execution/metadata are faked; no calculator code runs.
#include "app_list.hpp"
#include "target.hpp"
#if LAUNCHER_HH2
#include "loaders/hh2/loader.hpp"
#endif
#include "loaders/elf/loader.hpp"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace {
struct Entry { std::string name; bool directory = false; };
std::vector<Entry> files;
std::size_t cursor;
int searches, closes;
std::vector<std::string> patterns;

File_Error next(int handle, char_const16_t *name, File_FindInfo *info) {
  assert(handle == 42);
  while (cursor < files.size()) {
    const auto &entry = files[cursor++];
    // Simulate the OS wildcard selected by the production discovery function.
    const auto extension = patterns.back().substr(7);
    if (entry.name.size() < extension.size() ||
        !target::equals(std::string_view(entry.name).substr(entry.name.size() - extension.size()), extension)) continue;
    assert(entry.name.size() < 90);
    std::copy(entry.name.begin(), entry.name.end(), name);
    name[entry.name.size()] = 0;
    *info = {};
    info->type = entry.directory ? File_FindInfo::EntryTypeDirectory : File_FindInfo::EntryTypeFile;
    return FILE_OK;
  }
  return FILE_ENOENT;
}
File_Error first(const char_const16_t *pattern, int *handle,
                 char_const16_t *name, File_FindInfo *info) {
  std::string text;
  for (; *pattern; ++pattern) text.push_back(static_cast<char>(*pattern));
  patterns.push_back(text); ++searches; cursor = 0; *handle = 42;
  assert(text == (target::hh2 ? "\\fls0\\*.hhk" : "\\fls0\\*.hh3") || (target::hh2 && text == "\\fls0\\*.bin"));
  return next(*handle, name, info);
}
File_Error close_search(int handle) { assert(handle == 42); ++closes; return FILE_OK; }
void unexpected_debug(unsigned, unsigned, bool, int, const char *, ...) { assert(false); }
int unexpected_key() { assert(false); return 0; }
void unexpected_refresh() { assert(false); }
std::vector<std::string> scan(std::vector<Entry> entries) {
  files = std::move(entries); searches = closes = 0; patterns.clear();
  auto discovered = discover_apps();
  std::vector<std::string> paths;
  for (auto &app : discovered) paths.emplace_back(app->getPath().get());
  assert(searches == (target::hh2 ? 2 : 1) && closes == searches);
  return paths;
}
}

extern "C" {
File_Error (*File_FindFirst)(const char_const16_t *, int *, char_const16_t *, File_FindInfo *) = first;
File_Error (*File_FindNext)(int, char_const16_t *, File_FindInfo *) = next;
File_Error (*File_FindClose)(int) = close_search;
void (*Debug_Printf)(unsigned, unsigned, bool, int, const char *, ...) = unexpected_debug;
int (*Debug_WaitKey)() = unexpected_key;
void (*LCD_Refresh)() = unexpected_refresh;
}

void ELFLoader::load() { assert(false); }
void ELFLoader::unload() { assert(false); }
int ELFLoader::execute() { assert(false); return 0; }
std::unique_ptr<char[]> ELFLoader::getName() { return getPath(); }
std::unique_ptr<char[]> ELFLoader::getVersion() { return nullptr; }
std::unique_ptr<char[]> ELFLoader::getAuthor() { return nullptr; }
std::unique_ptr<char[]> ELFLoader::getDescription() { return nullptr; }

int main() {
#if LAUNCHER_HH2
  assert((scan({{"zeta.bin"},{"run.bin"},{"RUN.BIN"},{"run-hh3-os7002.bin"},{"old.hh3"},{"alpha.hhk"}}) == std::vector<std::string>{"\\fls0\\alpha.hhk","\\fls0\\zeta.bin"}));
  assert(scan({{"a.hh3"},{"run.bin"}}).empty());
  assert((scan({{"folder.hhk",true},{".hidden.bin"},{"visible.hhk"}}) == std::vector<std::string>{"\\fls0\\visible.hhk"}));
  std::vector<Entry> many;
  for (int i=999;i>=0;--i) many.push_back({"app"+std::to_string(i)+(i%2?".hhk":".bin")});
  const auto sorted=scan(std::move(many));
  assert(sorted.size()==1000 && std::is_sorted(sorted.begin(),sorted.end()));
  std::cout << "4/4 HH2 discovery checks passed: formats, launcher exclusion, filtering, 1000 apps\n";
#else
  assert((scan({{"zeta.hh3"}, {"old.hhk"}, {"run.bin"}, {"alpha.hh3"}, {"readme.txt"}}) ==
          std::vector<std::string>{"\\fls0\\alpha.hh3", "\\fls0\\zeta.hh3"}));
  std::cout << "PASS discovery: mixed directory lists and sorts only HH3 apps\n";
  assert(scan({{"old.hhk"}, {"older.bin"}, {"run.bin"}}).empty());
  std::cout << "PASS discovery: only unsupported files yields an empty app list\n";
  assert((scan({{"folder.hh3", true}, {".hidden.hh3"}, {"visible.hh3"}}) ==
          std::vector<std::string>{"\\fls0\\visible.hh3"}));
  std::cout << "PASS discovery: directories and hidden entries are excluded\n";
  std::vector<Entry> many;
  for (int i = 999; i >= 0; --i) many.push_back({"app" + std::to_string(i) + ".hh3"});
  const auto sorted = scan(std::move(many));
  assert(sorted.size() == 1000 && std::is_sorted(sorted.begin(), sorted.end()));
  std::cout << "PASS discovery: 1000 apps remain completely listed and sorted\n";
  std::cout << "4/4 discovery checks passed (mock file system; no app execution)\n";
#endif
}
