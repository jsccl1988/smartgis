// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_NET_ARCHIVE_H
#define SMT_NET_ARCHIVE_H

#include <cstddef>
#include <cstdint>
#include <ios>
#include <istream>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace net {

// Align with mogu base/archive BinarySink: reject huge length prefixes.
inline constexpr std::size_t k_binary_wire_max_string_bytes =
    64u * 1024u * 1024u;

// Binary SAX sink (mogu BinarySink subset). Native endian, size_t string prefix.
struct BinarySink {
  std::ostream* os = nullptr;
  std::istream* is = nullptr;

  template <typename Stream>
  void start_write(Stream&& stream) {
    writing_ = true;
    bind_write(stream);
  }

  void end_write() {}

  template <typename Stream>
  void start_read(Stream&& stream) {
    writing_ = false;
    bind_read(stream);
  }

  void end_read() {}

  void separate_field() {}

  template <typename T>
  void on_field() {}

  template <class T>
  void atom(T&& t) {
    using U = std::decay_t<T>;
    using Ref = std::remove_reference_t<T>;
    if constexpr (is_string_like_v<U>) {
      atom_string(std::forward<T>(t));
    } else if constexpr (std::is_arithmetic_v<U> || std::is_enum_v<U>) {
      if (writing_) {
        write_bytes(std::addressof(t), sizeof(U));
      } else if constexpr (!std::is_const_v<Ref>) {
        read_bytes(std::addressof(t), sizeof(U));
      }
    }
  }

  void atom(const std::string& s) { write_string(s.data(), s.size()); }
  void atom(std::string& s) {
    // archive() passes non-const members on both sides; direction is writing_.
    if (writing_) {
      write_string(s.data(), s.size());
    } else {
      read_string(s);
    }
  }
  void atom(std::string_view s) { write_string(s.data(), s.size()); }
  void atom(const char* s) {
    const std::string_view v(s ? s : "");
    write_string(v.data(), v.size());
  }

  bool ok() const { return ok_; }
  bool is_writing() const { return writing_; }

 private:
  template <class U>
  static constexpr bool is_string_like_v =
      std::is_same_v<U, std::string> || std::is_same_v<U, std::string_view> ||
      std::is_same_v<U, const char*> || std::is_same_v<U, char*>;

  bool writing_ = true;
  bool ok_ = true;
  void* raw_out_ = nullptr;
  void* raw_in_ = nullptr;
  void (*write_fn_)(void*, const char*, std::streamsize) = nullptr;
  std::streamsize (*read_fn_)(void*, char*, std::streamsize) = nullptr;

  template <typename Stream>
  void bind_write(Stream&& stream) {
    using S = std::remove_reference_t<Stream>;
    raw_out_ = std::addressof(stream);
    if constexpr (std::is_base_of_v<std::ostream, S>) {
      os = std::addressof(stream);
    }
    is = nullptr;
    write_fn_ = [](void* p, const char* d, std::streamsize n) {
      static_cast<S*>(p)->write(d, n);
    };
  }

  template <typename Stream>
  void bind_read(Stream&& stream) {
    using S = std::remove_reference_t<Stream>;
    raw_in_ = std::addressof(stream);
    if constexpr (std::is_base_of_v<std::istream, S>) {
      is = std::addressof(stream);
    }
    os = nullptr;
    read_fn_ = [](void* p, char* d, std::streamsize n) {
      static_cast<S*>(p)->read(d, n);
      return static_cast<S*>(p)->gcount();
    };
  }

  void write_bytes(const void* data, size_t n) {
    if (write_fn_ && n > 0) {
      write_fn_(raw_out_, static_cast<const char*>(data),
                static_cast<std::streamsize>(n));
    }
  }

  void read_bytes(void* data, size_t n) {
    if (!read_fn_ || n == 0) {
      return;
    }
    const std::streamsize got =
        read_fn_(raw_in_, static_cast<char*>(data), static_cast<std::streamsize>(n));
    if (got != static_cast<std::streamsize>(n)) {
      ok_ = false;
    }
  }

  void write_string(const char* data, size_t n) {
    atom(n);
    write_bytes(data, n);
  }

  void read_string(std::string& s) {
    size_t n = 0;
    atom(n);
    if (!ok_ || n > k_binary_wire_max_string_bytes) {
      ok_ = false;
      s.clear();
      return;
    }
    s.resize(n);
    read_bytes(s.data(), n);
  }

  template <class T>
  void atom_string(T&& t) {
    using U = std::decay_t<T>;
    if constexpr (std::is_same_v<U, std::string>) {
      if (writing_) {
        write_string(t.data(), t.size());
      } else if constexpr (std::is_lvalue_reference_v<T> &&
                           !std::is_const_v<std::remove_reference_t<T>>) {
        read_string(t);
      }
    } else if constexpr (std::is_same_v<U, std::string_view>) {
      write_string(t.data(), t.size());
    } else {
      const char* p = t;
      const std::string_view v(p ? p : "");
      write_string(v.data(), v.size());
    }
  }
};

template <typename Fmt>
struct FieldSink {
  Fmt* fmt = nullptr;

  template <typename... Ts>
  FieldSink& operator()(Ts&&... ts) {
    (emit(std::forward<Ts>(ts)), ...);
    return *this;
  }

  template <typename U>
  void emit(U&& u);
};

template <typename Fmt, typename T>
void write_value(Fmt& fmt, T&& t) {
  using U = std::decay_t<T>;
  if constexpr (std::is_arithmetic_v<U> || std::is_enum_v<U> ||
                std::is_same_v<U, std::string> || std::is_same_v<U, std::string_view> ||
                std::is_same_v<U, const char*>) {
    fmt.atom(std::forward<T>(t));
  } else {
    FieldSink<Fmt> fields{&fmt};
    const_cast<U&>(t).archive(fields);
  }
}

template <typename Fmt, typename T>
void read_value(Fmt& fmt, T& t) {
  using U = std::decay_t<T>;
  if constexpr (std::is_arithmetic_v<U> || std::is_enum_v<U> ||
                std::is_same_v<U, std::string>) {
    fmt.atom(t);
  } else {
    FieldSink<Fmt> fields{&fmt};
    t.archive(fields);
  }
}

template <typename Fmt>
template <typename U>
void FieldSink<Fmt>::emit(U&& u) {
  if (fmt->is_writing()) {
    write_value(*fmt, std::forward<U>(u));
  } else {
    read_value(*fmt, u);
  }
}

// OutArchiver / InArchiver: mogu base/archive subset (binary only).
template <typename Sink = BinarySink, typename OStream = std::ostream>
class OutArchiver {
 public:
  OutArchiver(Sink& format, OStream& stream) : format_(format), stream_(stream) {}

  template <typename... Ts>
  void operator()(Ts&&... ts) {
    format_.start_write(stream_);
    FieldSink<Sink> fields{&format_};
    fields(std::forward<Ts>(ts)...);
    format_.end_write();
  }

  Sink& format_;
  OStream& stream_;
};

template <typename Sink = BinarySink, typename IStream = std::istream>
class InArchiver {
 public:
  InArchiver(Sink& format, IStream& stream) : format_(format), stream_(stream) {}

  template <typename... Ts>
  void operator()(Ts&&... ts) {
    format_.start_read(stream_);
    FieldSink<Sink> fields{&format_};
    fields(std::forward<Ts>(ts)...);
    format_.end_read();
  }

  Sink& format_;
  IStream& stream_;
};

template <typename T>
struct binary_format_traits {
  template <typename Stream>
  static void write(Stream&& stream, const T& value) {
    using U = std::decay_t<T>;
    BinarySink fmt;
    if constexpr (std::is_arithmetic_v<U> || std::is_enum_v<U> ||
                  std::is_same_v<U, std::string> ||
                  std::is_same_v<U, std::string_view>) {
      fmt.start_write(stream);
      fmt.atom(value);
      fmt.end_write();
    } else {
      OutArchiver ar(fmt, stream);
      const_cast<U&>(value).archive(ar);
    }
  }

  template <typename Stream>
  static void read(Stream&& stream, T& value) {
    using U = std::decay_t<T>;
    BinarySink fmt;
    if constexpr (std::is_arithmetic_v<U> || std::is_enum_v<U> ||
                  std::is_same_v<U, std::string>) {
      fmt.start_read(stream);
      fmt.atom(value);
      fmt.end_read();
    } else {
      InArchiver ar(fmt, stream);
      value.archive(ar);
    }
  }
};

struct binary_format {
  template <typename Stream, typename T>
  static void write(Stream&& stream, T&& t) {
    using U = std::decay_t<T>;
    binary_format_traits<U>::write(std::forward<Stream>(stream), t);
  }

  template <typename Stream, typename T>
  static void read(Stream&& stream, T&& t) {
    using U = std::decay_t<T>;
    binary_format_traits<U>::read(std::forward<Stream>(stream), t);
  }
};

template <typename Format = binary_format, typename OStream = std::ostream>
class Serializer {
 public:
  explicit Serializer(OStream& stream) : stream_(stream) {}

  template <typename... Ts>
  void operator()(Ts&&... ts) {
    (write(std::forward<Ts>(ts)), ...);
  }

  template <typename T>
  Serializer& operator<<(T&& t) {
    write(std::forward<T>(t));
    return *this;
  }

  template <typename T>
  void write(T&& t) {
    Format::write(stream_, std::forward<T>(t));
  }

 private:
  OStream& stream_;
};

template <typename Format = binary_format, typename IStream = std::istream>
class Deserializer {
 public:
  explicit Deserializer(IStream& stream) : stream_(stream) {}

  template <typename... Ts>
  void operator()(Ts&&... ts) {
    (read(std::forward<Ts>(ts)), ...);
  }

  template <typename T>
  Deserializer& operator>>(T&& t) {
    read(std::forward<T>(t));
    return *this;
  }

  template <typename T>
  void read(T&& t) {
    Format::read(stream_, std::forward<T>(t));
  }

 private:
  IStream& stream_;
};

}  // namespace net

#endif  // SMT_NET_ARCHIVE_H
