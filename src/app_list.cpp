// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_list.hpp"
#include "target.hpp"
#if LAUNCHER_HH2
#include "loaders/hh2/loader.hpp"
#else
#include "loaders/elf/loader.hpp"
#endif
#include <cstring>

std::forward_list<std::unique_ptr<Executable>> discover_apps() {
  std::forward_list<std::unique_ptr<Executable>> list;
#if LAUNCHER_HH2
  discover<HHKLoader>::run(list);
  discover<BinaryLoader>::run(list);
#else
  discover<ELFLoader>::run(list);
#endif
  list.remove_if([](const auto &app) { return !target::accepts_path(app->pathView()); });
  list.sort([](const auto &a, const auto &b) {
    return a->pathView() < b->pathView();
  });
  return list;
}
