// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_NET_PICKLE_H
#define SMT_NET_PICKLE_H

#include "base/archive/archive.h"

#include <ios>
#include <memory>
#include <ostream>
#include <sstream>
#include <streambuf>
#include <string>
#include <utility>

namespace net {

// mogu net::pickle_t: write appends into a contiguous string; read copies inbound bytes.
class Pickle {
  struct ReadState {
    std::stringstream ss;
    mutable base::Deserializer<base::binary_format, std::stringstream> des;
    ReadState(const char* data, size_t size)
        : ss(std::string(data, size), std::ios::in | std::ios::binary), des(ss) {}
  };

  struct WriteState {
    std::string buffer;
    struct AppenderBuf : std::streambuf {
      std::string* s;
      explicit AppenderBuf(std::string* out) : s(out) { s->clear(); }
      std::streamsize xsputn(const char* p, std::streamsize n) override {
        s->append(p, static_cast<size_t>(n));
        return n;
      }
      int overflow(int c) override {
        using traits = std::char_traits<char>;
        if (traits::eq_int_type(c, traits::eof())) {
          return !traits::eof();
        }
        s->push_back(traits::to_char_type(c));
        return traits::not_eof(c);
      }
    } sbuf;
    std::ostream os;
    base::Serializer<base::binary_format, std::ostream> ser;
    WriteState() : sbuf(&buffer), os(&sbuf), ser(os) {}
  };

 public:
  Pickle() : read_(nullptr), write_(std::make_unique<WriteState>()) {}
  Pickle(const char* data, size_t size)
      : read_(std::make_unique<ReadState>(data, size)), write_(nullptr) {}

  Pickle(const Pickle&) = delete;
  Pickle& operator=(const Pickle&) = delete;
  Pickle(Pickle&&) noexcept = default;
  Pickle& operator=(Pickle&&) noexcept = default;

  std::string extract_wire() { return std::move(write_->buffer); }

  bool ok() const {
    if (!read_) {
      return false;
    }
    return !read_->ss.fail();
  }

  template <typename T>
  const Pickle& operator>>(T&& t) const {
    read_->des >> std::forward<T>(t);
    return *this;
  }

  template <typename T>
  Pickle& operator<<(T&& t) {
    write_->ser << std::forward<T>(t);
    return *this;
  }

 private:
  std::unique_ptr<ReadState> read_;
  std::unique_ptr<WriteState> write_;
};

}  // namespace net

#endif  // SMT_NET_PICKLE_H
