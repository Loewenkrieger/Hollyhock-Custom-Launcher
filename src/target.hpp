// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string_view>
#ifndef LAUNCHER_HH2
#define LAUNCHER_HH2 0
#endif
namespace target {
inline constexpr bool hh2 = LAUNCHER_HH2;
inline constexpr const char *name = hh2 ? "HH2" : "HH3";
inline constexpr const char *history_variable = hh2 ? "recent2" : "recent";
inline constexpr std::string_view history_signature = hh2 ? "HH2REC1\n" : "HH3REC1\n";
inline constexpr const char *empty_message = hh2
    ? "No apps were found on your calculator.\n\nCopy .hhk or compatible .bin files to the root directory of the calculator's flash."
    : "No apps were found on your calculator.\n\nCopy .hh3 files to the root directory of the calculator's flash.";
inline constexpr const char *footer = hh2 ? "HH2  |  0.6" : "HH3  |  0.6";
constexpr char lower(char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; }
constexpr bool equals(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) if (lower(a[i]) != lower(b[i])) return false;
  return true;
}
constexpr bool accepts_path(std::string_view path) {
  const auto file = path.substr(path.find_last_of("\\/") + 1);
  if (file.size() <= 4 || file.front() == '.') return false;
  const auto extension = file.substr(file.size() - 4);
  if (!hh2) return equals(extension, ".hh3");
  if (equals(extension, ".hhk")) return true;
  return equals(extension, ".bin") && !equals(file, "run.bin") &&
      !equals(file, "run-hh2-os2000.bin") && !equals(file, "run-hh3-os7002.bin");
}
}
