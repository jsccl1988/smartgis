// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "algorithm/proj/proj_runtime.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <sys/stat.h>
#include <windows.h>

#include "base/core/core.h"

namespace proj {
namespace detail {
namespace {

struct PipelineSlot {
  std::string source_def;
  std::string target_def;
  PJ* source_crs = nullptr;
  PJ* target_crs = nullptr;
  PJ* pipeline = nullptr;
};

PJ_CONTEXT* g_ctx = nullptr;
PipelineSlot g_by_def;
PipelineSlot g_by_pj;
std::string g_extra_search;

void destroy_slot(PipelineSlot* slot) {
  if (!slot) {
    return;
  }
  if (slot->pipeline) {
    proj_destroy(slot->pipeline);
    slot->pipeline = nullptr;
  }
  slot->source_crs = nullptr;
  slot->target_crs = nullptr;
  slot->source_def.clear();
  slot->target_def.clear();
}

std::string module_directory() {
  char path[MAX_PATH] = {};
  HMODULE module = nullptr;
  if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCSTR>(&module_directory),
                          &module)) {
    return {};
  }
  if (GetModuleFileNameA(module, path, MAX_PATH) == 0) {
    return {};
  }
  char* slash = std::strrchr(path, '\\');
  if (!slash) {
    slash = std::strrchr(path, '/');
  }
  if (slash) {
    *slash = '\0';
  }
  return path;
}

bool is_directory(const char* path) {
  if (!path || !path[0]) {
    return false;
  }
  struct _stat stat_buf = {};
  if (_stat(path, &stat_buf) != 0) {
    return false;
  }
  return (stat_buf.st_mode & _S_IFDIR) != 0;
}

void apply_search_paths(PJ_CONTEXT* ctx) {
  std::vector<std::string> owned;
  const std::string module_dir = module_directory();
  if (!module_dir.empty()) {
    owned.push_back(module_dir);
    owned.push_back(module_dir + "\\share\\proj");
  }
  if (!g_extra_search.empty()) {
    owned.push_back(g_extra_search);
  }
  const char* proj_data = std::getenv("PROJ_DATA");
  if (proj_data && proj_data[0]) {
    owned.push_back(proj_data);
  }
  const char* proj_lib = std::getenv("PROJ_LIB");
  if (proj_lib && proj_lib[0]) {
    owned.push_back(proj_lib);
  }

  std::vector<const char*> paths;
  paths.reserve(owned.size());
  for (const std::string& p : owned) {
    paths.push_back(p.c_str());
  }
  if (!paths.empty()) {
    proj_context_set_search_paths(ctx, static_cast<int>(paths.size()),
                                  paths.data());
  }
}

PJ* make_pipeline(PJ* source, PJ* target) {
  if (!source || !target) {
    return nullptr;
  }
  PJ* raw =
      proj_create_crs_to_crs_from_pj(context(), source, target, nullptr, nullptr);
  if (!raw) {
    return nullptr;
  }
  PJ* vis = proj_normalize_for_visualization(context(), raw);
  if (vis) {
    proj_destroy(raw);
    return vis;
  }
  return raw;
}

}  // namespace

double gauss_kruger_central_meridian(double lon_deg) {
  double lon = lon_deg;
  while (lon < 0.0) {
    lon += 360.0;
  }
  while (lon >= 360.0) {
    lon -= 360.0;
  }
  const int zone = static_cast<int>(lon / 6.0) + 1;
  return static_cast<double>(zone) * 6.0 - 3.0;
}

std::string make_longlat_crs(double a, double b) {
  char buf[160];
  std::snprintf(buf, sizeof(buf), "+proj=longlat +a=%.10f +b=%.10f +type=crs", a,
                b);
  return buf;
}

std::string make_tmerc_crs(double a, double b, double lon_0) {
  char buf[320];
  std::snprintf(buf, sizeof(buf),
                "+proj=tmerc +lat_0=0 +lon_0=%.8f +k=1 +x_0=500000 +y_0=0 "
                "+a=%.10f +b=%.10f +units=m +type=crs",
                lon_0, a, b);
  return buf;
}

std::string make_lcc_crs(double a,
                         double b,
                         double lat_1,
                         double lat_2,
                         double lat_0,
                         double lon_0) {
  char buf[320];
  std::snprintf(buf, sizeof(buf),
                "+proj=lcc +lat_1=%.8f +lat_2=%.8f +lat_0=%.8f +lon_0=%.8f "
                "+x_0=0 +y_0=0 +a=%.10f +b=%.10f +units=m +type=crs",
                lat_1, lat_2, lat_0, lon_0, a, b);
  return buf;
}

PJ_CONTEXT* context() {
  if (!g_ctx) {
    g_ctx = proj_context_create();
    if (g_ctx) {
      apply_search_paths(g_ctx);
    }
  }
  return g_ctx;
}

void set_search_path(const char* proj_lib, const char* rel_to_path) {
  g_extra_search.clear();
  if (!proj_lib || !proj_lib[0]) {
    if (g_ctx) {
      apply_search_paths(g_ctx);
    }
    return;
  }

  const bool absolute = proj_lib[0] == '/' || proj_lib[0] == '\\' ||
                        (proj_lib[0] != '\0' && proj_lib[1] == ':');
  if (!absolute && rel_to_path && rel_to_path[0]) {
    std::string extended = std::string(rel_to_path) + "\\" + proj_lib;
    if (is_directory(extended.c_str())) {
      g_extra_search = std::move(extended);
    } else {
      g_extra_search = proj_lib;
    }
  } else {
    g_extra_search = proj_lib;
  }

  if (!g_ctx) {
    context();
    return;
  }
  apply_search_paths(g_ctx);
}

PJ* create_crs(const char* definition) {
  if (!definition || !definition[0]) {
    return nullptr;
  }
  return proj_create(context(), definition);
}

PJ* cached_pipeline(const char* source_crs, const char* target_crs) {
  if (!source_crs || !target_crs) {
    return nullptr;
  }
  if (g_by_def.pipeline && g_by_def.source_def == source_crs &&
      g_by_def.target_def == target_crs) {
    return g_by_def.pipeline;
  }
  destroy_slot(&g_by_def);

  PJ* source = create_crs(source_crs);
  PJ* target = create_crs(target_crs);
  PJ* pipeline = make_pipeline(source, target);
  if (source) {
    proj_destroy(source);
  }
  if (target) {
    proj_destroy(target);
  }
  if (!pipeline) {
    return nullptr;
  }
  g_by_def.source_def = source_crs;
  g_by_def.target_def = target_crs;
  g_by_def.pipeline = pipeline;
  return pipeline;
}

PJ* cached_pipeline_from_pj(PJ* source_crs, PJ* target_crs) {
  if (!source_crs || !target_crs) {
    return nullptr;
  }
  if (g_by_pj.pipeline && g_by_pj.source_crs == source_crs &&
      g_by_pj.target_crs == target_crs) {
    return g_by_pj.pipeline;
  }
  destroy_slot(&g_by_pj);
  PJ* pipeline = make_pipeline(source_crs, target_crs);
  if (!pipeline) {
    return nullptr;
  }
  g_by_pj.source_crs = source_crs;
  g_by_pj.target_crs = target_crs;
  g_by_pj.pipeline = pipeline;
  return pipeline;
}

void forget_pj(PJ* crs) {
  if (!crs) {
    return;
  }
  if (g_by_pj.source_crs == crs || g_by_pj.target_crs == crs) {
    destroy_slot(&g_by_pj);
  }
}

int trans_xy(PJ* pipeline, PJ_DIRECTION direction, double* x, double* y) {
  if (!pipeline || !x || !y) {
    return SMT_ERR_INVALID_PARAM;
  }
  proj_errno_reset(pipeline);
  PJ_COORD in = proj_coord(*x, *y, 0.0, 0.0);
  PJ_COORD out = proj_trans(pipeline, direction, in);
  if (proj_errno(pipeline) != 0 || !std::isfinite(out.xy.x) ||
      !std::isfinite(out.xy.y)) {
    return SMT_ERR_FAILURE;
  }
  *x = out.xy.x;
  *y = out.xy.y;
  return SMT_ERR_NONE;
}

bool is_geographic_crs(const PJ* crs) {
  if (!crs) {
    return false;
  }
  const PJ_TYPE type = proj_get_type(crs);
  return type == PJ_TYPE_GEOGRAPHIC_CRS || type == PJ_TYPE_GEOGRAPHIC_2D_CRS ||
         type == PJ_TYPE_GEOGRAPHIC_3D_CRS;
}

}  // namespace detail
}  // namespace proj
