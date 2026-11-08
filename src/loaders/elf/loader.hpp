#pragma once

#include "../interface.hpp"
#include "elfnotes.hpp"
#include <elf.h>
#include <memory>
#include <vector>

class ELFLoader : public FileBasedExecutable<".hh3"> {
public:
  using FileBasedExecutable::FileBasedExecutable;
  void load() override;
  void unload() override;
  int execute() override;

  std::unique_ptr<char[]> getName() override;
  std::unique_ptr<char[]> getAuthor() override;
  std::unique_ptr<char[]> getDescription() override;
  std::unique_ptr<char[]> getVersion() override;

private:
  bool loaded = false;
  std::unique_ptr<Elf32_Ehdr> ehdr = nullptr;
  std::unique_ptr<Elf32_Phdr[]> phdrs = nullptr;
  // Note views borrow these stable segment allocations; no name/description
  // buffers or file handles are retained separately for each note.
  std::vector<std::unique_ptr<std::uint8_t[]>> note_segments;
  std::unique_ptr<std::forward_list<ELFNotes>> notes = nullptr;

  void init_ehdr();
  void init_phdrs();
  void init_notes();
};
