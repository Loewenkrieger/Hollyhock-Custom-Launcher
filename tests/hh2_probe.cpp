// SPDX-License-Identifier: GPL-3.0-or-later
#include "loaders/hh2/image.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc, char **argv) {
  if (argc != 3) return 2;
  try {
    std::ifstream input(argv[2], std::ios::binary);
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input), {}};
    const auto image = std::string(argv[1]) == "bin" ? hh2::parse_bin(bytes) : hh2::parse_hhk(bytes);
    std::cout << std::hex << image.entry << " " << image.name << "\n";
    for (const auto &s : image.sections) {
      std::cout << std::hex << s.address << ":" << s.size << ":";
      for (auto byte : s.data) { const char hex[]="0123456789abcdef"; std::cout << hex[byte>>4] << hex[byte&15]; }
      std::cout << "\n";
    }
    return 0;
  } catch (const std::exception &error) { std::cerr << error.what(); return 1; }
}
