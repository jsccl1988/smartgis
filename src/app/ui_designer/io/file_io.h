// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_UI_DESIGNER_IO_FILE_IO_H_
#define APP_UI_DESIGNER_IO_FILE_IO_H_

#include <cstdint>
#include <string>

namespace app {

// UTF-8 path mtime (seconds); 0 if the file cannot be stated.
uint64_t file_mtime(const std::string& path);

// Reads an entire file as bytes via a UTF-8 path (wide fopen on Windows).
std::string read_file_utf8(const std::string& path);

}  // namespace app

#endif  // APP_UI_DESIGNER_IO_FILE_IO_H_
