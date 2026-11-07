// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <vector>
namespace hh2 {
struct Section {
  std::uint32_t address, size, flags;
  std::vector<std::uint8_t> data;
};
struct Image {
  std::uint32_t entry = 0;
  std::string name, author, version, description;
  std::vector<Section> sections;
};
// Validate the entire image before touching calculator RAM.
Image parse_hhk(std::span<const std::uint8_t> bytes);
Image parse_bin(std::span<const std::uint8_t> bytes);
Image read_metadata(std::FILE *file, bool binary);
bool app_range(std::uint32_t address, std::uint32_t size);
}
