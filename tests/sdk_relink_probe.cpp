// SPDX-License-Identifier: GPL-3.0-or-later
// Use the unchanged SDK resolver against a mapped ROM guard and generated table.
// Host table entries use host pointer width; target tests check the SH table.
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sys/mman.h>
#include <vector>
extern "C" bool relink_sdk(const std::uint8_t *,std::size_t);
extern void *LCD_Refresh;
extern void *MCS_GetVariable;
extern "C" void calcInit() {}
extern "C" void calcExit() {}
int main(int argc,char **argv) {
  assert(argc == 5);
  void *page = mmap(reinterpret_cast<void *>(0x814ff000),4096,PROT_READ|PROT_WRITE,
      MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
  assert(page == reinterpret_cast<void *>(0x814ff000));
  std::strcpy(reinterpret_cast<char *>(0x814fffe0),argv[1]);
  std::ifstream input(argv[2],std::ios::binary);
  std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input),{}};
  assert(relink_sdk(bytes.data(),bytes.size()));
  assert(reinterpret_cast<std::uintptr_t>(LCD_Refresh) == std::strtoull(argv[3],nullptr,16));
  assert(reinterpret_cast<std::uintptr_t>(MCS_GetVariable) == std::strtoull(argv[4],nullptr,16));
  auto *before = LCD_Refresh;
  bytes[0] = 'X';
  assert(!relink_sdk(bytes.data(),bytes.size()) && LCD_Refresh == before);
  munmap(page,4096);
  std::cout << "PASS unchanged SDK resolver binds selected OS symbols and rejects a different ROM guard\n";
}
