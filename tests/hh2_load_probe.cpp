// SPDX-License-Identifier: GPL-3.0-or-later
// Fixed app RAM overlaps ASan shadow on x86-64; this probe uses UBSan.
#include "loaders/hh2/loader.hpp"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sys/mman.h>
std::size_t bytes_read = 0;
extern "C" std::size_t __real_fread(void *,std::size_t,std::size_t,std::FILE *);
extern "C" std::size_t __wrap_fread(void *out,std::size_t size,std::size_t count,std::FILE *file) {
  const auto result = __real_fread(out,size,count,file);
  bytes_read += size * result; return result;
}
int main(int argc, char **argv) {
  assert(argc == 3);
  auto *ram = static_cast<unsigned char *>(mmap(reinterpret_cast<void *>(0x8cff0000),
      0x10000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0));
  assert(ram == reinterpret_cast<void *>(0x8cff0000));
  std::memset(ram,0xa5,0x10000);
  HH2Executable app(argv[2], std::string(argv[1]) == "bin");
  try { (void)app.execute(); assert(false); } catch (const std::runtime_error &) {}
  const auto name = app.getName();
  assert(name && app.getAuthor() && app.getDescription() && app.getVersion());
  if (std::string(argv[1]) == "metadata") {
    assert(std::strcmp(name.get(), "Fixture") == 0 && bytes_read < 8192);
    std::cout << "PASS large HHK menu inspection reads only cached metadata, not app code\n";
    munmap(ram,0x10000); return 0;
  }
  if (std::string(argv[1]) == "invalid") {
    try { app.load(); assert(false); } catch (const std::runtime_error &) {}
    for (unsigned i=0;i<0x10000;++i) assert(ram[i] == 0xa5);
    std::cout << "PASS invalid HHK leaves app RAM untouched\n";
  } else {
    app.load();
    if (std::string(argv[1]) == "bin") assert(ram[0] == 0x00 && ram[1] == 0x09);
    else {
      assert(ram[0] == 0xd1 && ram[1] == 0x01);
      for (unsigned i=0x20;i<0x28;++i) assert(ram[i] == 0);
      assert(ram[0x28] == 0xa5);
    }
    app.unload();
    try { (void)app.execute(); assert(false); } catch (const std::runtime_error &) {}
    std::cout << "PASS real HH2 loader copies validated bytes, zeroes BSS and resets execution state\n";
  }
  munmap(ram,0x10000);
}
