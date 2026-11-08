// SPDX-License-Identifier: GPL-3.0-or-later
// Real loader on mapped app RAM; a host trampoline checks the entry ABI.
// This is not SH instruction execution. Fixed mappings use UBSan, not ASan.
#include "loaders/elf/loader.hpp"
#include <bit>
#include <cassert>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <sys/mman.h>
#include <unistd.h>
namespace {
constexpr std::uint32_t base = 0x8cff0000;
std::string expected_path;
char **expected_env;
int calls;
int entry(int argc,char **argv,char **envp) {
  assert(argc == 1 && argv && !argv[1] && expected_path == argv[0]);
  assert(envp == expected_env && std::strncmp(envp[0],"HHK_SYMBOL_TABLE=",17) == 0 &&
         std::strncmp(envp[1],"HHK_SYMBOL_TABLE_LEN=",21) == 0 && !envp[2]);
  ++calls; return 37;
}
std::vector<std::uint8_t> program() {
  std::vector<std::uint8_t> bytes{0x48,0xb8};
  const auto pointer = reinterpret_cast<std::uintptr_t>(entry);
  for (unsigned i=0;i<8;++i) bytes.push_back((pointer>>(8*i))&255);
  bytes.push_back(0xff); bytes.push_back(0xe0);
  return bytes;
}
void write(const std::string &path,const std::function<void(Elf32_Ehdr &,std::vector<Elf32_Phdr> &)> &change) {
  const auto code = program();
  Elf32_Ehdr header{};
  std::memcpy(header.e_ident,ELFMAG,SELFMAG);
  header.e_ident[EI_CLASS] = ELFCLASS32;
  header.e_ident[EI_DATA] = std::endian::native == std::endian::little ? ELFDATA2LSB : ELFDATA2MSB;
  header.e_ident[EI_VERSION] = EV_CURRENT;
  header.e_type = ET_EXEC; header.e_machine = EM_SH; header.e_version = EV_CURRENT;
  header.e_entry = base; header.e_ehsize = sizeof(header); header.e_phoff = sizeof(header);
  header.e_phentsize = sizeof(Elf32_Phdr); header.e_phnum = 3;
  std::vector<Elf32_Phdr> parts(3);
  const auto offset = sizeof(header)+parts.size()*sizeof(Elf32_Phdr);
  parts[0] = {PT_LOAD,static_cast<Elf32_Off>(offset),base,base,static_cast<Elf32_Word>(code.size()),static_cast<Elf32_Word>(code.size()),PF_R|PF_X,4};
  parts[1] = {PT_LOAD,static_cast<Elf32_Off>(offset+code.size()),base+0x100,base+0x100,4,12,PF_R|PF_W,4};
  parts[2] = {PT_LOAD,0,base+0x200,base+0x200,0,0,PF_R,4};
  change(header,parts);
  std::ofstream file(path,std::ios::binary);
  file.write(reinterpret_cast<const char *>(&header),sizeof(header));
  file.write(reinterpret_cast<const char *>(parts.data()),parts.size()*sizeof(Elf32_Phdr));
  file.write(reinterpret_cast<const char *>(code.data()),code.size());
  file.write("DATA",4);
}
}
int main(int argc,char **argv) {
  assert(argc == 2);
  expected_path = argv[1];
  auto *ram = static_cast<unsigned char *>(mmap(reinterpret_cast<void *>(base),0x10000,
      PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0));
  assert(ram == reinterpret_cast<void *>(base));
  char table[] = "HHK_SYMBOL_TABLE=12345678";
  char length[] = "HHK_SYMBOL_TABLE_LEN=10";
  char *environment[]{table,length,nullptr};
  auto **original_env = environ;
  expected_env = environment; environ = environment;
  const auto unchanged = [](auto &,auto &) {};
  write(expected_path,unchanged);
  ELFLoader app(argv[1]);
  try { (void)app.execute(); assert(false); } catch (const std::runtime_error &) {}
  std::memset(ram,0xa5,0x10000);
  app.load();
  assert(std::memcmp(ram,program().data(),program().size()) == 0);
  assert(std::memcmp(ram+0x100,"DATA",4) == 0);
  for (unsigned i=0x104;i<0x10c;++i) assert(ram[i] == 0);
  assert(ram[0x10c] == 0xa5 && ram[0x200] == 0xa5);
  std::cout << "PASS real HH3 loader copies segments, zeroes BSS and ignores zero-length segments\n";
#if defined(__x86_64__)
  assert(app.execute() == 37 && calls == 1);
  std::cout << "PASS real HH3 entry call passes argc, original filename and SDK environment\n";
#endif
  app.unload();
  try { (void)app.execute(); assert(false); } catch (const std::runtime_error &) {}
  std::cout << "PASS HH3 execution requires a successful load and is disabled after unload\n";
  const std::vector<std::function<void(Elf32_Ehdr &,std::vector<Elf32_Phdr> &)>> invalid{
    [](auto &h,auto &) { h.e_machine = EM_386; },
    [](auto &h,auto &) { h.e_type = ET_DYN; },
    [](auto &h,auto &) { h.e_entry = base+1; },
    [](auto &h,auto &) { h.e_entry = base+0x100; },
    [](auto &,auto &p) { p[1].p_vaddr = 0x8cfd0000; },
    [](auto &,auto &p) { p[1].p_vaddr = 0x8cfffffc; },
    [](auto &,auto &p) { p[1].p_memsz = 1; },
    [](auto &,auto &p) { p[1].p_offset = 0xfffffff0; },
    [](auto &,auto &p) { p[1].p_flags = 0x80; }
  };
  for (const auto &change : invalid) {
    write(expected_path,unchanged);
    ELFLoader bad(argv[1]);
    (void)bad.getName(); // Cache metadata before replacing the file.
    write(expected_path,change);
    std::memset(ram,0xa5,0x10000);
    try { bad.load(); assert(false); } catch (const std::runtime_error &) {}
    for (unsigned i=0;i<0x10000;++i) assert(ram[i] == 0xa5);
    try { (void)bad.execute(); assert(false); } catch (const std::runtime_error &) {}
  }
  std::cout << "PASS wrong CPU/type/entry, reserved RAM, overflow, sizes, flags and truncated segments fail before any write\n";
  environ = original_env;
  munmap(ram,0x10000);
}
