// SPDX-License-Identifier: GPL-3.0-or-later
#include "loader.hpp"
#include <cstring>
#include <stdexcept>
namespace {
std::unique_ptr<char[]> copy(const std::string &text) {
  auto out = std::make_unique<char[]>(text.size() + 1);
  std::memcpy(out.get(), text.c_str(), text.size() + 1);
  return out;
}
void flush(std::uintptr_t begin, std::size_t size) {
#if defined(__sh__)
  for (auto p = begin & ~std::uintptr_t(31); p < begin + size; p += 32) {
    __asm__ volatile("ocbwb @%0" : : "r"(p) : "memory");
    __asm__ volatile("icbi @%0" : : "r"(p) : "memory");
  }
#else
  (void)begin; (void)size;
#endif
}
}
hh2::Image HH2Executable::read_image() {
  auto opened = file.acquire();
  auto *f = opened.get();
  if (std::fseek(f, 0, SEEK_END) != 0) throw std::runtime_error("HH2: seek failed");
  const auto size = std::ftell(f);
  if (size < 2 || size > 16 * 1024 * 1024) throw std::runtime_error("HH2: invalid file size");
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
  if (std::fseek(f, 0, SEEK_SET) != 0 || std::fread(bytes.data(), 1, bytes.size(), f) != bytes.size())
    throw std::runtime_error("HH2: file read failed");
  return binary ? hh2::parse_bin(bytes) : hh2::parse_hhk(bytes);
}
void HH2Executable::inspect() {
  if (inspected) return;
  inspected = true;
  try {
    const auto opened = file.acquire();
    const auto image = hh2::read_metadata(opened.get(), binary);
    name = image.name.empty() ? path : image.name;
    author = image.author; version = image.version; description = image.description;
  } catch (const std::exception &e) {
    name = path; description = std::string("Cannot start this HH2 app:\n") + e.what();
  }
}
std::unique_ptr<char[]> HH2Executable::getName() { inspect(); return copy(name); }
std::unique_ptr<char[]> HH2Executable::getAuthor() { inspect(); return copy(author); }
std::unique_ptr<char[]> HH2Executable::getVersion() { inspect(); return copy(version); }
std::unique_ptr<char[]> HH2Executable::getDescription() { inspect(); return copy(description); }
void HH2Executable::load() {
  loaded = false;
  const auto image = read_image();
  for (const auto &section : image.sections) {
    auto *out = reinterpret_cast<void *>(section.address);
    if (!section.data.empty()) std::memcpy(out, section.data.data(), section.data.size());
    if (section.size > section.data.size())
      std::memset(reinterpret_cast<void *>(section.address + section.data.size()), 0, section.size - section.data.size());
    flush(section.address, section.size);
  }
  entry = image.entry;
  loaded = true;
}
int HH2Executable::execute() {
  if (!loaded) throw std::runtime_error("HH2: app not loaded");
  reinterpret_cast<void (*)()>(entry)();
  return 0;
}
