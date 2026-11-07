// SPDX-License-Identifier: GPL-3.0-or-later
#include "recent.hpp"
#include "target.hpp"
#include <algorithm>
#include <cstring>
#include <iterator>
#include <sdk/os/mcs.h>

namespace recent {
namespace {
constexpr char folder[] = "hhlaunch";
constexpr auto variable = target::history_variable;
constexpr auto signature = target::history_signature;
constexpr std::size_t max_path = 99, max_record = 512;

bool valid_path(std::string_view path) {
  constexpr std::string_view root = "\\fls0\\";
  if (!path.starts_with(root) || path.size() <= root.size() + 4 || path.size() > max_path)
    return false;
  if (!target::accepts_path(path)) return false;
  for (const auto c : path.substr(root.size())) {
    const auto byte = static_cast<unsigned char>(c);
    if (byte < 32 || byte == 127 || c == '\\' || c == '/') return false;
  }
  return true;
}
bool contains(const History &history, std::string_view path) {
  for (std::size_t i = 0; i < history.size; ++i)
    if (history.paths[i] == path) return true;
  return false;
}
}

History load() noexcept {
  try {
    History history;
    if (!MCS_GetVariableU) return history;
    // OS 7002 writes one byte; with GCC 15.2.0 the SDK enum is four bytes.
    // Use the SDK's byte declaration. Evidence and target test: docs/MCS-ABI.md.
    std::uint8_t type = 0;
    char *name = nullptr;
    void *data = nullptr;
    std::uint32_t size = 0;
    if (MCS_GetVariableU(folder, variable, &type, &name, &data, &size) != MCS_OK ||
        type != VARTYPE_STR || !data || !size || size > max_record)
      return history;
    const auto *text = static_cast<const char *>(data);
    const auto *end = static_cast<const char *>(std::memchr(text, '\0', size));
    if (!end) return history;
    std::string_view record(text, end - text);
    if (!record.starts_with(signature)) return history;
    record.remove_prefix(signature.size());
    while (!record.empty() && history.size < capacity) {
      const auto newline = record.find('\n');
      if (newline == std::string_view::npos) break;
      const auto path = record.substr(0, newline);
      if (valid_path(path) && !contains(history, path))
        history.paths[history.size++] = path;
      record.remove_prefix(newline + 1);
    }
    return history;
  } catch (...) {
    return {};
  }
}

std::size_t prioritize(std::forward_list<std::unique_ptr<Executable>> &apps,
                       History &history) {
  History existing;
  auto insert = apps.before_begin();
  for (std::size_t i = 0; i < history.size; ++i) {
    auto before = insert;
    for (auto item = std::next(before); item != apps.end(); ++item, ++before) {
      if ((*item)->pathView() != history.paths[i]) continue;
      if (before != insert) apps.splice_after(insert, apps, before);
      ++insert;
      existing.paths[existing.size++] = std::move(history.paths[i]);
      break;
    }
  }
  history = std::move(existing);
  return history.size;
}

void remember(std::string_view path, History &history) noexcept {
  try {
    if (!valid_path(path) || !MCS_CreateFolder || !MCS_SetVariableU) return;
    History updated;
    updated.paths[updated.size++] = path;
    for (std::size_t i = 0; i < history.size && updated.size < capacity; ++i)
      if (valid_path(history.paths[i]) && !contains(updated, history.paths[i]))
        updated.paths[updated.size++] = history.paths[i];
    std::array<char, max_record> record{};
    auto *out = std::copy(signature.begin(), signature.end(), record.data());
    for (std::size_t i = 0; i < updated.size; ++i) {
      out = std::copy(updated.paths[i].begin(), updated.paths[i].end(), out);
      *out++ = '\n';
    }
    *out++ = '\0';
    std::uint8_t index = 0;
    const auto status = MCS_CreateFolder(folder, &index);
    if (status != MCS_OK && status != MCS_FOLDER_EXISTS) return;
    if (MCS_SetVariableU(folder, variable, VARTYPE_STR,
                        static_cast<std::uint32_t>(out - record.data()),
                        record.data()) == MCS_OK)
      history = std::move(updated);
  } catch (...) {
    // History must never prevent a successfully loaded app from running.
  }
}
} // namespace recent
