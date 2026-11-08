#include "loader.hpp"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <elf.h>
#include <stdexcept>
#include <unistd.h>

namespace {
bool app_range(std::uint32_t address, std::uint32_t size) {
  return size && address >= 0x8c000000 && address < 0x8d000000 &&
      size <= 0x8d000000 - address &&
      (address + size <= 0x8cfd0000 || address >= 0x8cff0000);
}
void flush(std::uintptr_t address, std::size_t size, bool executable) {
#if defined(__sh__)
  for (auto p = address & ~std::uintptr_t(31); p < address + size; p += 32) {
    __asm__ volatile("ocbwb @%0" : : "r"(p) : "memory");
    if (executable) __asm__ volatile("icbi @%0" : : "r"(p) : "memory");
  }
#else
  (void)address; (void)size; (void)executable;
#endif
}
}

void ELFLoader::load() {
  loaded = false;
  const auto shared = file.acquire();
  auto *file = shared.get();
  // Menu metadata may have been inspected earlier. Re-read the load headers.
  phdrs.reset();
  ehdr.reset();
  init_phdrs();
  if ((ehdr->e_type != ET_EXEC && ehdr->e_type != ET_CORE) || ehdr->e_machine != EM_SH)
    throw std::runtime_error("Expected a static SuperH executable");
  if (std::fseek(file,0,SEEK_END) != 0) throw std::runtime_error("Failed to seek to ELF end");
  const auto length = std::ftell(file);
  if (length < 0) throw std::runtime_error("Failed to read ELF size");
  bool has_entry = false;
  // Validate all segments before the first RAM write.
  for (const auto &p : std::span(phdrs.get(),ehdr->e_phnum)) {
    if (p.p_type != PT_LOAD) continue;
    if (p.p_filesz > p.p_memsz || (p.p_flags & ~(PF_X | PF_W | PF_R)))
      throw std::runtime_error("Illegal ELF segment sizes or flags");
    if (!p.p_memsz || !p.p_flags) continue;
    if (!app_range(p.p_vaddr,p.p_memsz))
      throw std::runtime_error("ELF segment overlaps reserved RAM");
    if (p.p_filesz && (p.p_offset > static_cast<unsigned long>(length) ||
        p.p_filesz > static_cast<unsigned long>(length) - p.p_offset))
      throw std::runtime_error("ELF segment outside file");
    if ((p.p_flags & PF_X) && ehdr->e_entry >= p.p_vaddr &&
        ehdr->e_entry < p.p_vaddr + p.p_memsz)
      has_entry = true;
  }
  if (!has_entry || (ehdr->e_entry & 1))
    throw std::runtime_error("ELF entry point outside executable RAM");

  for (const auto &p : std::span(phdrs.get(),ehdr->e_phnum)) {
    if (p.p_type != PT_LOAD || !p.p_memsz || !p.p_flags) continue;
    if (p.p_filesz && (std::fseek(file,p.p_offset,SEEK_SET) != 0 ||
        std::fread(reinterpret_cast<void *>(p.p_vaddr),p.p_filesz,1,file) != 1))
      throw std::runtime_error("Failed to read executable contents");
    std::memset(reinterpret_cast<void *>(p.p_vaddr + p.p_filesz),0,p.p_memsz - p.p_filesz);
    flush(p.p_vaddr,p.p_memsz,p.p_flags & PF_X);
  }
  loaded = true;
}

void ELFLoader::unload() { loaded = false; }

int ELFLoader::execute() {
  if (!loaded) throw std::runtime_error("HH3 app not loaded");
  const auto argv0 = getPath();
  char *argv[]{argv0.get(),nullptr};
  return reinterpret_cast<int (*)(int,char **,char **)>(ehdr->e_entry)(1,argv,environ);
}
