#pragma once

#include "../strlit.hpp"
#include "LazyFile.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <forward_list>
#include <memory>
#include <new>
#include <sdk/os/debug.h>
#include <sdk/os/file.h>
#include <sdk/os/lcd.h>
#include <string>
#include <string_view>
#include <type_traits>

class Executable {
public:
  virtual void load() = 0;
  virtual void unload() = 0;
  virtual int execute() = 0;
  virtual std::unique_ptr<char[]> getName() { return getPath(); }
  virtual std::string_view pathView() const = 0;
  std::unique_ptr<char[]> getPath() {
    const auto value = pathView();
    auto result = std::make_unique<char[]>(value.size() + 1);
    std::copy(value.begin(), value.end(), result.get());
    result[value.size()] = '\0';
    return result;
  }
  virtual std::unique_ptr<char[]> getVersion() { return nullptr; }
  virtual std::unique_ptr<char[]> getAuthor() { return nullptr; }
  virtual std::unique_ptr<char[]> getDescription() { return nullptr; }
  virtual ~Executable() = default;
};

template <StringLiteral EXT> class FileBasedExecutable : public Executable {
protected:
  const std::unique_ptr<const char[]> path;
  LazyFile file;

public:
  template <typename CharT>
  explicit FileBasedExecutable(const CharT *path)
      : path([path] {
          auto len = std::char_traits<CharT>::length(path) + 1;
          auto uptr = std::make_unique<char[]>(len);
          std::copy_n(path, len, uptr.get());
          return uptr;
        }()),
        file(this->path.get(), "rb") {}
  std::string_view pathView() const override { return path.get(); }

  static constexpr auto ext = EXT;
};
