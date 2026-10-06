// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/gate.h"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <vector>

namespace app {
namespace detail {
namespace {

struct GridStats {
  int samples = 0;
  int lit = 0;
  long sum = 0;
};

// Reject buffers that cannot hold a BGR(A) pixel at the last sample.
bool stride_holds_row(int stride, int w, int bpp) {
  if (stride <= 0 || w < 1 || bpp < 3 || bpp > 8) {
    return false;
  }
  if (w > 16384) {
    return false;
  }
  return stride >= w * bpp;
}

const unsigned char* row_at(const unsigned char* pixels, int stride, int y) {
  return pixels + static_cast<size_t>(y) * static_cast<size_t>(stride);
}

GridStats sample_grid(const unsigned char* pixels,
                      int stride,
                      int w,
                      int h,
                      int bpp) {
  GridStats out;
  if (!pixels || h < 1 || !stride_holds_row(stride, w, bpp)) {
    return out;
  }
  const int step_x = std::max(1, w / 32);
  const int step_y = std::max(1, h / 24);
  for (int y = 0; y < h; y += step_y) {
    const unsigned char* row = row_at(pixels, stride, y);
    for (int x = 0; x < w; x += step_x) {
      const int off = x * bpp;
      if (off + 2 >= stride) {
        break;
      }
      const unsigned char b = row[off + 0];
      const unsigned char g = row[off + 1];
      const unsigned char r = row[off + 2];
      ++out.samples;
      out.sum += static_cast<long>(r) + g + b;
      if (static_cast<int>(r) + g + b > 24) {
        ++out.lit;
      }
    }
  }
  return out;
}

bool grid_lit_ok(const GridStats& stats) {
  return stats.samples > 0 && stats.lit * 20 >= stats.samples;
}

int count_unique_colors(const unsigned char* pixels,
                        int stride,
                        int w,
                        int h,
                        int bpp) {
  int unique = 0;
  unsigned char seen[64][3] = {};
  const int step_x = std::max(1, w / 24);
  const int step_y = std::max(1, h / 18);
  for (int y = 0; y < h; y += step_y) {
    const unsigned char* row = row_at(pixels, stride, y);
    for (int x = 0; x < w; x += step_x) {
      const int off = x * bpp;
      if (off + 2 >= stride) {
        break;
      }
      const unsigned char b = row[off + 0];
      const unsigned char g = row[off + 1];
      const unsigned char r = row[off + 2];
      bool found = false;
      for (int i = 0; i < unique; ++i) {
        if (seen[i][0] == r && seen[i][1] == g && seen[i][2] == b) {
          found = true;
          break;
        }
      }
      if (!found && unique < 64) {
        seen[unique][0] = r;
        seen[unique][1] = g;
        seen[unique][2] = b;
        ++unique;
      }
    }
  }
  return unique;
}

bool sparse_distinct(const unsigned char* pixels, int stride, int w, int h) {
  if (!stride_holds_row(stride, w, 3)) {
    return false;
  }
  int non_near_black = 0;
  int distinct = 0;
  unsigned last = 0xFFFFFFFFu;
  for (int y = 0; y < h; y += 4) {
    const unsigned char* row = row_at(pixels, stride, y);
    for (int x = 0; x < w; x += 4) {
      const int off = x * 3;
      if (off + 2 >= stride) {
        break;
      }
      const unsigned char b = row[off + 0];
      const unsigned char g = row[off + 1];
      const unsigned char r = row[off + 2];
      if (r > 24 || g > 24 || b > 24) {
        ++non_near_black;
      }
      const unsigned packed = (static_cast<unsigned>(r) << 16) |
                              (static_cast<unsigned>(g) << 8) |
                              static_cast<unsigned>(b);
      if (packed != last) {
        ++distinct;
        last = packed;
      }
    }
  }
  return non_near_black > 32 && distinct > 4;
}

bool read_bmp_pixels(FILE* in,
                     BITMAPFILEHEADER* fh,
                     BITMAPINFOHEADER* bi,
                     std::vector<unsigned char>* pixels,
                     int* out_w,
                     int* out_h,
                     const BmpFileCheckOpts& opts) {
  if (std::fread(fh, sizeof(*fh), 1, in) != 1 ||
      std::fread(bi, sizeof(*bi), 1, in) != 1 || fh->bfType != 0x4D42) {
    return false;
  }
  if (bi->biSize < sizeof(BITMAPINFOHEADER) ||
      (bi->biCompression != BI_RGB && bi->biCompression != BI_BITFIELDS) ||
      bi->biWidth <= 0 || bi->biHeight == 0 || bi->biHeight == INT_MIN) {
    return false;
  }
  if (fh->bfOffBits < sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)) {
    return false;
  }
  const int w = bi->biWidth;
  const int h = bi->biHeight < 0 ? -bi->biHeight : bi->biHeight;
  if (out_w) {
    *out_w = w;
  }
  if (out_h) {
    *out_h = h;
  }
  if (w < opts.min_w || h < opts.min_h || w > kMaxBmpEdge || h > kMaxBmpEdge) {
    return false;
  }
  if (bi->biBitCount == 24) {
    // ok
  } else if (opts.allow_32bpp && bi->biBitCount == 32) {
    // ok
  } else {
    return false;
  }
  const int stride = ((w * bi->biBitCount + 31) / 32) * 4;
  if (stride <= 0) {
    return false;
  }
  const size_t stride_sz = static_cast<size_t>(stride);
  const size_t nbytes = stride_sz * static_cast<size_t>(h);
  if (nbytes / stride_sz != static_cast<size_t>(h)) {
    return false;
  }
  pixels->assign(nbytes, 0);
  if (std::fseek(in, static_cast<long>(fh->bfOffBits), SEEK_SET) != 0 ||
      std::fread(pixels->data(), 1, pixels->size(), in) != pixels->size()) {
    return false;
  }
  return true;
}

bool bmp_stream_has_visible_signal(FILE* in,
                                   int* out_w,
                                   int* out_h,
                                   const BmpFileCheckOpts& opts) {
  BITMAPFILEHEADER fh{};
  BITMAPINFOHEADER bi{};
  std::vector<unsigned char> pixels;
  if (!read_bmp_pixels(in, &fh, &bi, &pixels, out_w, out_h, opts)) {
    return false;
  }
  const int w = bi.biWidth;
  const int h = bi.biHeight < 0 ? -bi.biHeight : bi.biHeight;
  const int bpp = bi.biBitCount / 8;
  const int stride = ((w * bi.biBitCount + 31) / 32) * 4;
  PixelGate gate;
  gate.require_color_diversity = opts.require_color_diversity;
  gate.mean_min = opts.mean_min;
  gate.mean_max = opts.mean_max;
  return pixels_pass_gate(pixels.data(), stride, w, h, bpp, gate);
}

bool check_open_file(FILE* in,
                     int* out_w,
                     int* out_h,
                     const BmpFileCheckOpts& opts) {
  if (!in) {
    return false;
  }
  const bool ok = bmp_stream_has_visible_signal(in, out_w, out_h, opts);
  std::fclose(in);
  return ok;
}

}  // namespace

bool pixels_have_visible_signal(const unsigned char* pixels,
                                int stride,
                                int w,
                                int h,
                                VisiblePolicy policy) {
  if (!pixels || stride <= 0 || w < 8 || h < 8) {
    return false;
  }
  if (policy == VisiblePolicy::kSparseDistinct) {
    return sparse_distinct(pixels, stride, w, h);
  }
  // kGridLitFraction — atmosphere grid (>=5% lit); require a larger frame.
  if (w < 32 || h < 32) {
    return false;
  }
  return grid_lit_ok(sample_grid(pixels, stride, w, h, 3));
}

bool bmp_has_shell_diversity(const unsigned char* pixels,
                              int stride,
                              int w,
                              int h) {
  if (!pixels || w < 8 || h < 8 || !stride_holds_row(stride, w, 3)) {
    return false;
  }
  int buckets[64] = {};
  int used = 0;
  int accent = 0;
  for (int y = 0; y < h; y += 8) {
    const unsigned char* row = row_at(pixels, stride, y);
    for (int x = 0; x < w; x += 8) {
      const int off = x * 3;
      if (off + 2 >= stride) {
        break;
      }
      const unsigned char b = row[off + 0];
      const unsigned char g = row[off + 1];
      const unsigned char r = row[off + 2];
      const int idx = ((r >> 6) << 4) | ((g >> 6) << 2) | (b >> 6);
      if (buckets[idx] == 0) {
        ++used;
      }
      ++buckets[idx];
      // Active Map tab accent (#007acc) required for a finished shell paint.
      if (r < 40 && g > 90 && g < 160 && b > 170 && b > r + 100) {
        ++accent;
      }
    }
  }
  return used >= 4 && accent >= 6;
}

bool pixels_pass_gate(const unsigned char* pixels,
                      int stride,
                      int w,
                      int h,
                      int bpp,
                      const PixelGate& gate) {
  const GridStats stats = sample_grid(pixels, stride, w, h, bpp);
  if (!grid_lit_ok(stats)) {
    return false;
  }
  if (gate.mean_min > 0.0 || gate.mean_max > 0.0) {
    if (stats.samples < 8) {
      return false;
    }
    const double mean = static_cast<double>(stats.sum) /
                        (static_cast<double>(stats.samples) * 3.0);
    if (gate.mean_min > 0.0 && mean <= gate.mean_min) {
      return false;
    }
    if (gate.mean_max > 0.0 && !(mean < gate.mean_max)) {
      return false;
    }
  }
  if (!gate.require_color_diversity) {
    return true;
  }
  return count_unique_colors(pixels, stride, w, h, bpp) >= 2;
}

bool bmp_file_has_visible_signal(const wchar_t* filename,
                                 int* out_w,
                                 int* out_h,
                                 const BmpFileCheckOpts& opts) {
  if (!filename) {
    return false;
  }
  FILE* in = nullptr;
  if (_wfopen_s(&in, filename, L"rb") != 0) {
    return false;
  }
  return check_open_file(in, out_w, out_h, opts);
}

bool bmp_file_has_visible_signal_a(const char* filename,
                                   int* out_w,
                                   int* out_h,
                                   const BmpFileCheckOpts& opts) {
  if (!filename) {
    return false;
  }
  FILE* in = nullptr;
  if (fopen_s(&in, filename, "rb") != 0) {
    return false;
  }
  return check_open_file(in, out_w, out_h, opts);
}

}  // namespace detail
}  // namespace app
