// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CODEGEN_IO_H_
#define IL_RUNTIME_CODEGEN_IO_H_

#include <string>

namespace app {
namespace detail {

bool file_exists_wide(const std::wstring& path);

// Reads |path| as bytes into |out|. Fails if the file is missing, empty, or
// larger than |max_bytes|.
bool read_utf8_file(const std::wstring& path,
                    std::string* out,
                    size_t max_bytes);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_CODEGEN_IO_H_
