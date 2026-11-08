// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_list.hpp"
#include "target.hpp"
#include "loaders/elf/loader.hpp"
#if LAUNCHER_HH2
#include "loaders/hh2/loader.hpp"
#endif
#include <array>

namespace {
struct FindHandle {
  int value;
  ~FindHandle() { File_FindClose(value); }
};
}

std::forward_list<std::unique_ptr<Executable>> discover_apps() {
  std::forward_list<std::unique_ptr<Executable>> list;
  // The SDK supports "*" for a full directory scan. Match extensions here,
  // independently of the OS wildcard implementation and filename case.
  constexpr char_const16_t pattern[]{'\\','f','l','s','0','\\','*',0};
  std::array<char_const16_t,100> name{};
  File_FindInfo info{};
  int handle = 0;
  auto result = File_FindFirst(pattern, &handle, name.data(), &info);
  if (result < FILE_OK) return list;
  const FindHandle search{handle};
  do {
    const auto end = std::find(name.begin(), name.end(), 0);
    if (info.type == File_FindInfo::EntryTypeFile && end != name.end()) {
      std::string path = "\\fls0\\";
      for (auto p = name.begin(); p != end; ++p) path.push_back(static_cast<char>(*p));
      if (target::accepts_path(path)) {
        const std::string_view extension(path.data() + path.size() - 4,4);
        if (target::equals(extension,".hh3"))
          list.emplace_front(std::make_unique<ELFLoader>(path.c_str()));
#if LAUNCHER_HH2
        else if (target::equals(extension,".hhk"))
          list.emplace_front(std::make_unique<HHKLoader>(path.c_str()));
        else
          list.emplace_front(std::make_unique<BinaryLoader>(path.c_str()));
#endif
      }
    }
    name.fill(0);
    info = {};
    result = File_FindNext(handle,name.data(),&info);
  } while (result >= FILE_OK);
  list.sort([](const auto &a, const auto &b) { return a->pathView() < b->pathView(); });
  return list;
}
