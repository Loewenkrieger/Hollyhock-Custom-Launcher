// SPDX-License-Identifier: GPL-3.0-or-later
#include "image.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>
namespace hh2 {
namespace {
void require(bool ok, const char *message) {
  if (!ok) throw std::runtime_error(message);
}
void bounds(std::span<const std::uint8_t> b, std::size_t o, std::size_t n) {
  require(o <= b.size() && n <= b.size() - o, "HH2: truncated file");
}
std::uint16_t u16(std::span<const std::uint8_t> b, std::size_t o) {
  bounds(b, o, 2); return (std::uint16_t(b[o]) << 8) | b[o + 1];
}
std::uint32_t u32(std::span<const std::uint8_t> b, std::size_t o) {
  bounds(b, o, 4);
  return (std::uint32_t(b[o]) << 24) | (std::uint32_t(b[o+1]) << 16) |
         (std::uint32_t(b[o+2]) << 8) | b[o+3];
}
std::string text(std::span<const std::uint8_t> b, std::size_t o) {
  bounds(b, o, 1);
  const auto end = std::find(b.begin() + o, b.end(), 0);
  require(end != b.end() && static_cast<std::size_t>(end - b.begin() - o) <= 4096,
          "HH2: invalid metadata string");
  return std::string(reinterpret_cast<const char *>(b.data() + o), end - b.begin() - o);
}
struct Header { std::uint32_t name, type, flags, address, offset, size; };
}
bool app_range(std::uint32_t address, std::uint32_t size) {
  // Keep the OS workspace, launcher, and the top of physical RAM untouched.
  // The lower window supports larger native apps (e.g. custom BIN linkers).
  return size && address >= 0x8c200000 && address < 0x8d000000 &&
      size <= 0x8d000000 - address &&
      (address + size <= 0x8cfd0000 || address >= 0x8cff0000);
}
Image parse_hhk(std::span<const std::uint8_t> b) {
  constexpr std::array<std::uint8_t, 8> magic{127,'E','L','F',1,2,1,0};
  require(b.size() >= 52 && b.size() <= 16 * 1024 * 1024, "HH2: invalid ELF size");
  require(std::equal(magic.begin(), magic.end(), b.begin()), "HH2: expected big-endian ELF32 SYSV");
  require(u16(b,16) == 2 && u16(b,18) == 42 && u32(b,20) == 1,
          "HH2: expected static SuperH executable");
  require(u16(b,40) == 52 && u16(b,46) == 40, "HH2: invalid ELF header sizes");
  const auto count = u16(b,48), names = u16(b,50);
  require(count && count <= 1024 && names < count, "HH2: invalid section table");
  const auto table = u32(b,32);
  bounds(b, table, std::size_t(count) * 40);
  std::vector<Header> headers;
  for (std::size_t i = 0; i < count; ++i) {
    const auto o = table + i * 40;
    Header h{u32(b,o),u32(b,o+4),u32(b,o+8),u32(b,o+12),u32(b,o+16),u32(b,o+20)};
    if (h.type != 8) bounds(b, h.offset, h.size);
    headers.push_back(h);
  }
  std::span<const std::uint8_t> strings;
  if (names) {
    const auto &h = headers[names];
    require(h.type == 3, "HH2: invalid section-name table");
    strings = b.subspan(h.offset, h.size);
  }
  Image image;
  image.entry = u32(b,24);
  for (const auto &h : headers) {
    const auto name = strings.empty() ? std::string() : text(strings, h.name);
    if (name.starts_with(".hollyhock_")) {
      require(h.type == 1 && h.size && h.size <= 4096, "HH2: invalid app metadata");
      const auto value = text(b.subspan(h.offset, h.size), 0);
      if (name == ".hollyhock_name") image.name = value;
      if (name == ".hollyhock_author") image.author = value;
      if (name == ".hollyhock_version") image.version = value;
      if (name == ".hollyhock_description") image.description = value;
    }
    if (!(h.flags & 2) || !h.size) continue;
    require(h.type == 1 || h.type == 8, "HH2: unsupported allocated section");
    require(app_range(h.address, h.size), "HH2: section overlaps reserved RAM");
    require(!(h.address & 1), "HH2: misaligned section");
    for (const auto &s : image.sections)
      require(h.address + h.size <= s.address || s.address + s.size <= h.address,
              "HH2: overlapping sections");
    Section section{h.address,h.size,h.flags,{}};
    if (h.type == 1) section.data.assign(b.begin() + h.offset, b.begin() + h.offset + h.size);
    image.sections.push_back(std::move(section));
  }
  require(!(image.entry & 1) && std::any_of(image.sections.begin(), image.sections.end(),
      [&](const auto &s) { return (s.flags & 4) && image.entry >= s.address &&
                                image.entry < s.address + s.size; }),
      "HH2: entry point outside executable RAM");
  return image;
}
Image parse_bin(std::span<const std::uint8_t> b) {
  require(b.size() >= 2 && b.size() <= 16 * 1024 * 1024, "HH2: invalid BIN size");
  const std::uint32_t address = b.size() >= 16 && b[12] == 0x8c ? u32(b,12) : 0x8cff0000;
  require(!(address & 1) && app_range(address, b.size()), "HH2: BIN overlaps reserved RAM");
  Image image;
  image.entry = address;
  image.sections.push_back({address,static_cast<std::uint32_t>(b.size()),6,{b.begin(),b.end()}});
  // Optional SnailMath BIN header: four padded NUL-terminated strings at 0x10.
  // Treat missing or non-text headers as ordinary binaries, not load errors.
  if (b.size() > 16 && b[16] >= 32 && b[16] < 127) {
    std::array<std::string,4> fields;
    std::size_t offset = 16;
    bool valid = true;
    for (auto &field : fields) {
      const auto begin = offset;
      while (offset < b.size() && b[offset] && offset - begin < 4096) {
        if ((b[offset] < 32 && b[offset] != '\n' && b[offset] != '\r' && b[offset] != '\t') || b[offset] == 127)
          valid = false;
        ++offset;
      }
      if (offset == b.size() || b[offset]) { valid = false; break; }
      field.assign(reinterpret_cast<const char *>(b.data() + begin), offset - begin);
      while (offset < b.size() && !b[offset]) ++offset;
    }
    if (valid) {
      image.name = fields[0]; image.description = fields[1];
      image.author = fields[2]; image.version = fields[3];
    }
  }
  return image;
}
Image read_metadata(std::FILE *file, bool binary) {
  require(std::fseek(file, 0, SEEK_END) == 0, "HH2: metadata seek failed");
  const auto length = std::ftell(file);
  require(length >= 2 && length <= 16 * 1024 * 1024, "HH2: invalid file size");
  const auto size = static_cast<std::size_t>(length);
  const auto read = [&](std::size_t offset, std::size_t count) {
    require(offset <= size && count <= size - offset, "HH2: truncated metadata");
    std::vector<std::uint8_t> bytes(count);
    require(std::fseek(file, offset, SEEK_SET) == 0 &&
        (!count || std::fread(bytes.data(), 1, count, file) == count), "HH2: metadata read failed");
    return bytes;
  };
  if (binary) return parse_bin(read(0, std::min(size, std::size_t(16 + 4 * 4097))));
  const auto header = read(0,52);
  constexpr std::array<std::uint8_t,8> magic{127,'E','L','F',1,2,1,0};
  require(std::equal(magic.begin(), magic.end(), header.begin()) && u16(header,16) == 2 &&
      u16(header,18) == 42 && u16(header,46) == 40, "HH2: invalid ELF metadata header");
  const auto count = u16(header,48), names = u16(header,50);
  require(count && count <= 1024 && names < count, "HH2: invalid metadata table");
  Image image;
  if (!names) return image;
  const auto table = read(u32(header,32), std::size_t(count) * 40);
  const auto name_offset = std::size_t(names) * 40;
  require(u32(table,name_offset+4) == 3 && u32(table,name_offset+20) <= 1024 * 1024,
          "HH2: invalid section-name table");
  const auto strings = read(u32(table,name_offset+16), u32(table,name_offset+20));
  for (std::size_t i = 0; i < count; ++i) {
    const auto offset = i * 40;
    const auto name = text(strings, u32(table,offset));
    if (!name.starts_with(".hollyhock_")) continue;
    const auto bytes = u32(table,offset+20);
    require(u32(table,offset+4) == 1 && bytes && bytes <= 4096, "HH2: invalid app metadata");
    const auto value = text(read(u32(table,offset+16),bytes),0);
    if (name == ".hollyhock_name") image.name = value;
    if (name == ".hollyhock_author") image.author = value;
    if (name == ".hollyhock_version") image.version = value;
    if (name == ".hollyhock_description") image.description = value;
  }
  return image;
}
}
