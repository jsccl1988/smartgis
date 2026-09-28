// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_MAP_REDUCE_IO_H
#define BASE_EXECUTION_MAP_REDUCE_IO_H

#include <fstream>
#include <functional>
#include <string>

namespace base {
namespace execution {
class RecordReader {
 public:
  RecordReader(const std::string& file_name) : if_(file_name) {}
  ~RecordReader() {
    if (if_.is_open()) {
      if_.close();
    }
  }

  template <typename Fn>
  void read(Fn&& fn) {
    std::string str;
    while (std::getline(if_, str)) {
      fn(str);
    }
  }

 private:
  std::ifstream if_;
};

class RecordWriter {
 public:
  RecordWriter(const std::string& file_name) : of_(file_name) {}
  ~RecordWriter() {
    if (of_.is_open()) {
      of_.close();
    }
  }

  template <typename... Ts>
  void write(Ts&&... ts) {
    base::OutArchiver oar(format_, of_);
    oar(std::forward<Ts>(ts)...);
  }

 private:
  base::TextFormat format_;
  std::ofstream of_;
};
}  // namespace execution
}  // namespace base
#endif  // BASE_EXECUTION_MAP_REDUCE_IO_H
