// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_RUNTIME_INTERACT_IO_IO_H_
#define APP_VIEWS_RUNTIME_INTERACT_IO_IO_H_

#include <string>

namespace app {
namespace detail {

bool utf8_to_wide(const std::string& utf8, std::wstring* out);

bool wide_to_utf8(const std::wstring& wide, std::string* out);

bool file_exists_wide(const std::wstring& path);

// Reads |path| as bytes into |out|. Fails if the file is missing, empty, or
// larger than |max_bytes|.
bool read_utf8_file(const std::wstring& path,
                    std::string* out,
                    size_t max_bytes);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_RUNTIME_INTERACT_IO_IO_H_
