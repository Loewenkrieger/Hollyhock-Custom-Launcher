// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../interface.hpp"
#include "image.hpp"
#include <optional>
class HH2Executable : public Executable {
  std::string path;
  LazyFile file;
  bool binary;
  bool inspected = false, loaded = false;
  std::string name, author, version, description;
  std::uint32_t entry = 0;
  hh2::Image read_image();
  void inspect();
public:
  HH2Executable(const char *path, bool binary) : path(path), file(this->path.c_str(), "rb"), binary(binary) {}
  std::string_view pathView() const override { return path; }
  void load() override;
  void unload() override { loaded = false; }
  int execute() override;
  std::unique_ptr<char[]> getName() override;
  std::unique_ptr<char[]> getVersion() override;
  std::unique_ptr<char[]> getAuthor() override;
  std::unique_ptr<char[]> getDescription() override;
};
template <StringLiteral EXT, bool BIN> class HH2Loader : public FileBasedExecutable<EXT> {
  HH2Executable app;
public:
  template<typename CharT> explicit HH2Loader(const CharT *path)
      : FileBasedExecutable<EXT>(path), app(this->path.get(), BIN) {}
  void load() override { app.load(); }
  void unload() override { app.unload(); }
  int execute() override { return app.execute(); }
  std::unique_ptr<char[]> getName() override { return app.getName(); }
  std::unique_ptr<char[]> getVersion() override { return app.getVersion(); }
  std::unique_ptr<char[]> getAuthor() override { return app.getAuthor(); }
  std::unique_ptr<char[]> getDescription() override { return app.getDescription(); }
};
using HHKLoader = HH2Loader<".hhk", false>;
using BinaryLoader = HH2Loader<".bin", true>;
