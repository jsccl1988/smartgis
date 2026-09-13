// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/dem/tin_loader.h"

#include <cstdio>
#include <fstream>
#include <locale>
#include <string>
#include <vector>

#include "algorithm/tin/tin.h"
#include "base/core/core.h"
#include "render/math/math.h"

namespace plugin {

long load_ascii_xyz_tin(const char* file_name,
                        const SmtTinFileFmt& file_fmt,
                        float x_scale,
                        float y_scale,
                        float z_scale,
                        Smt3DSurface* out_surf) {
  if (!file_name || !out_surf)
    return SMT_ERR_FAILURE;

  std::fstream fin;
  const std::locale loc1 = std::locale::global(std::locale(".936"));
  fin.open(file_name, std::ios::in);
  std::locale::global(std::locale(loc1));

  if (!fin.is_open())
    return SMT_ERR_INVALID_FILE;

  char sz_buf[2000];
  render::Vector3 ver;
  std::vector<render::Vector3> verts;

  int n_head_skip = 0;
  while (!fin.eof() && n_head_skip < file_fmt.nHeadSkip) {
    fin.getline(sz_buf, 2000, '\n');
    ++n_head_skip;
  }

  char ch_separator = ',';
  switch (file_fmt.nSeparatorType) {
    case ST_TAB:
      ch_separator = '\t';
      break;
    case ST_SPACE:
      ch_separator = ' ';
      break;
    case ST_COMMA:
    default:
      ch_separator = ',';
      break;
  }

  std::string str_float = "%f";
  std::string str_filter = "%*[^";
  str_float += ch_separator;
  str_filter += ch_separator;
  str_filter += "]";
  str_filter += ch_separator;

  std::string str_parse_fmt;
  for (int i = 0; i < file_fmt.nCol; ++i) {
    if (i == file_fmt.iX || i == file_fmt.iY || i == file_fmt.iZ)
      str_parse_fmt += str_float;
    else
      str_parse_fmt += str_filter;
  }

  while (!fin.eof()) {
    int n_line_skip = file_fmt.nLineSkip + 1;
    while (!fin.eof() && n_line_skip > 0) {
      fin.getline(sz_buf, 2000, '\n');
      --n_line_skip;
    }

    if (sscanf(sz_buf, str_parse_fmt.c_str(), &ver.x, &ver.y, &ver.z) != 3)
      if (sscanf(sz_buf, "%*[^,],,%f,%f,%f", &ver.x, &ver.y, &ver.z) != 3)
        continue;

    ver.x *= x_scale;
    ver.y *= y_scale;
    ver.z *= z_scale;
    verts.push_back(ver);
  }

  fin.close();

  if (verts.size() < 3)
    return SMT_ERR_INVALID_FILE;

  out_surf->is_empty();
  if (SMT_ERR_NONE == create_delaunay_tin_div(out_surf, verts))
    return SMT_ERR_NONE;

  return SMT_ERR_INVALID_FILE;
}

}  // namespace plugin
