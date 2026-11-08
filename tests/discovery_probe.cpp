// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_list.hpp"
#include "target.hpp"
#include "loaders/elf/loader.hpp"
#if LAUNCHER_HH2
#include "loaders/hh2/loader.hpp"
#endif
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct Entry { std::string name; bool directory = false; };
std::vector<Entry> files;
std::size_t cursor;
int searches, closes;
bool fail_first, fail_next, throw_next, unterminated;
File_Error take(int handle,char_const16_t *name,File_FindInfo *info) {
  assert(handle == 42);
  if (cursor == files.size()) return FILE_ENOENT;
  const auto &entry = files[cursor++];
  assert(entry.name.size() < 100);
  std::copy(entry.name.begin(),entry.name.end(),name);
  name[entry.name.size()] = 0;
  if (unterminated) std::fill_n(name,100,'A');
  *info = {};
  info->type = entry.directory ? File_FindInfo::EntryTypeDirectory : File_FindInfo::EntryTypeFile;
  return FILE_OK;
}
File_Error first(const char_const16_t *pattern,int *handle,char_const16_t *name,File_FindInfo *info) {
  std::string text;
  for (;*pattern;++pattern) text.push_back(static_cast<char>(*pattern));
  // No case-folding or extension matching by the fake OS: enumerate all names.
  assert(text == "\\fls0\\*");
  ++searches; cursor = 0;
  if (fail_first) return FILE_EDEVFAIL;
  *handle = 42;
  return take(*handle,name,info);
}
File_Error next(int handle,char_const16_t *name,File_FindInfo *info) {
  if (throw_next) throw std::runtime_error("interrupted enumeration");
  if (fail_next) return FILE_EDEVFAIL;
  return take(handle,name,info);
}
File_Error close_search(int handle) { assert(handle == 42); ++closes; return FILE_OK; }
void reset() { searches = closes = 0; fail_first = fail_next = throw_next = unterminated = false; }
std::vector<std::string> scan(std::vector<Entry> entries) {
  files = std::move(entries);
  const auto apps = discover_apps();
  std::vector<std::string> paths;
  for (const auto &app : apps) {
    const auto path = app->pathView();
    const auto extension = path.substr(path.size()-4);
    if (target::equals(extension,".hh3")) assert(dynamic_cast<ELFLoader *>(app.get()));
#if LAUNCHER_HH2
    else if (target::equals(extension,".hhk")) assert(dynamic_cast<HHKLoader *>(app.get()));
    else assert(dynamic_cast<BinaryLoader *>(app.get()));
#endif
    paths.emplace_back(path);
  }
  assert(searches == 1 && closes == (files.empty() || fail_first ? 0 : 1));
  return paths;
}
}
extern "C" {
File_Error (*File_FindFirst)(const char_const16_t *,int *,char_const16_t *,File_FindInfo *) = first;
File_Error (*File_FindNext)(int,char_const16_t *,File_FindInfo *) = next;
File_Error (*File_FindClose)(int) = close_search;
}
void ELFLoader::load() { assert(false); }
void ELFLoader::unload() { assert(false); }
int ELFLoader::execute() { assert(false); return 0; }
std::unique_ptr<char[]> ELFLoader::getName() { return getPath(); }
std::unique_ptr<char[]> ELFLoader::getVersion() { return nullptr; }
std::unique_ptr<char[]> ELFLoader::getAuthor() { return nullptr; }
std::unique_ptr<char[]> ELFLoader::getDescription() { return nullptr; }

int main() {
  reset();
  const auto mixed = scan({{"old.HHK"},{"demo.Hh3"},{"game.BIN"},{"run.bin"},{"RUN.BIN"},
      {"run-hh2-os2000.bin"},{"run-hh3-os7002.bin"},{"readme.txt"}});
  const std::vector<std::string> expected = target::hh2
      ? std::vector<std::string>{"\\fls0\\demo.Hh3","\\fls0\\game.BIN","\\fls0\\old.HHK"}
      : std::vector<std::string>{"\\fls0\\demo.Hh3"};
  assert(mixed == expected);
  std::cout << "PASS mixed HHK/HH3/BIN names use the correct loaders and exclude launcher files\n";

  reset();
  const auto cases = scan({{"a.HH3"},{"b.Hh3"},{"c.hh3"},{"d.HHK"},{"e.HhK"},{"f.hhk"},
                          {"g.BIN"},{"h.Bin"},{"i.bin"}});
  assert(cases.size() == (target::hh2 ? 9 : 3));
  std::cout << "PASS extension recognition does not depend on OS wildcard case behavior\n";

  reset();
  assert((scan({{"folder.hh3",true},{".hidden.hh3"},{"visible.hh3"},{".hidden.hhk"},
                {"folder.bin",true}}) == std::vector<std::string>{"\\fls0\\visible.hh3"}));
  std::cout << "PASS hidden files, directories and unsupported entries stay excluded\n";

  reset();
  assert(scan({}).empty());
  std::cout << "PASS an empty directory does not close an unopened find handle\n";
  reset(); fail_first = true;
  assert(scan({{"app.hh3"}}).empty());
  std::cout << "PASS a failed initial search does not close an unopened handle\n";

  reset(); fail_next = true;
  assert(scan({{"first.hh3"},{"second.hh3"}}).size() == 1);
  std::cout << "PASS a failed continuation closes the active search exactly once\n";
  reset(); throw_next = true; files = {{"first.hh3"}};
  try { (void)discover_apps(); assert(false); } catch (const std::runtime_error &) {}
  assert(searches == 1 && closes == 1);
  std::cout << "PASS active searches are closed when discovery is interrupted\n";

  reset(); unterminated = true;
  assert(scan({{"broken.hh3"}}).empty());
  std::cout << "PASS unterminated OS filenames are ignored within buffer bounds\n";

  reset();
  std::vector<Entry> many;
  for (int i=999;i>=0;--i) {
    const char *ext = !target::hh2 || i%3 == 0 ? ".HH3" : i%3 == 1 ? ".HhK" : ".BIN";
    many.push_back({"app"+std::to_string(i)+ext});
  }
  const auto sorted = scan(std::move(many));
  assert(sorted.size() == 1000 && std::is_sorted(sorted.begin(),sorted.end()));
  std::cout << "PASS one directory pass lists and sorts all 1000 supported apps\n";
}
