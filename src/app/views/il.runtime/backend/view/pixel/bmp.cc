// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/view/pixel/bmp.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace app {
namespace detail {

bool write_bmp_file(const wchar_t* path,
                    const BITMAPINFOHEADER& bi,
                    const void* pixels,
                    size_t nbytes) {
  if (!path || !pixels || nbytes == 0 ||
      nbytes > static_cast<size_t>(0xFFFFFFFFu) - sizeof(BITMAPFILEHEADER) -
                    sizeof(BITMAPINFOHEADER)) {
    return false;
  }
  BITMAPFILEHEADER fh{};
  fh.bfType = 0x4D42;
  fh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  fh.bfSize = fh.bfOffBits + static_cast<DWORD>(nbytes);
  FILE* out = nullptr;
  if (_wfopen_s(&out, path, L"wb") != 0 || !out) {
    return false;
  }
  const bool ok = std::fwrite(&fh, sizeof(fh), 1, out) == 1 &&
                  std::fwrite(&bi, sizeof(bi), 1, out) == 1 &&
                  std::fwrite(pixels, 1, nbytes, out) == nbytes;
  std::fclose(out);
  if (!ok) {
    DeleteFileW(path);
  }
  return ok;
}

bool write_engine_sidecar(const wchar_t* bmp_w, const char* engine) {
  if (!bmp_w || !engine) {
    return false;
  }
  std::wstring path(bmp_w);
  const size_t dot = path.find_last_of(L'.');
  if (dot != std::wstring::npos) {
    path.resize(dot);
  }
  path += L".engine.txt";
  FILE* f = nullptr;
  if (_wfopen_s(&f, path.c_str(), L"wb") != 0 || !f) {
    return false;
  }
  const bool ok = std::fwrite(engine, 1, std::strlen(engine), f) ==
                      std::strlen(engine) &&
                  std::fputc('\n', f) != EOF;
  std::fclose(f);
  return ok;
}

}  // namespace detail
}  // namespace app
