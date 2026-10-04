// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/geo/proj/coordinate_transform.h"

#include <proj.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>

namespace geo {
namespace {

struct PjContextDeleter {
  void operator()(PJ_CONTEXT* ctx) const {
    if (ctx) {
      proj_context_destroy(ctx);
    }
  }
};

struct PjDeleter {
  void operator()(PJ* object) const {
    if (object) {
      proj_destroy(object);
    }
  }
};

using PjContextPtr = std::unique_ptr<PJ_CONTEXT, PjContextDeleter>;
using PjPtr = std::unique_ptr<PJ, PjDeleter>;

std::mutex g_proj_mu;

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

void apply_search_paths(PJ_CONTEXT* ctx) {
  if (!ctx) {
    return;
  }
  std::vector<std::string> owned;
  const std::string module_dir = module_directory();
  if (!module_dir.empty()) {
    owned.push_back(module_dir);
    owned.push_back(module_dir + "\\share\\proj");
  }
  if (const char* proj_data = std::getenv("PROJ_DATA");
      proj_data && proj_data[0]) {
    owned.push_back(proj_data);
  }
  if (const char* proj_lib = std::getenv("PROJ_LIB"); proj_lib && proj_lib[0]) {
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

PJ_CONTEXT* context_locked() {
  static PjContextPtr ctx;
  if (!ctx) {
    ctx.reset(proj_context_create());
    apply_search_paths(ctx.get());
  }
  return ctx.get();
}

PjPtr make_pipeline_locked(const std::string& source, const std::string& target) {
  PJ_CONTEXT* ctx = context_locked();
  if (!ctx || source.empty() || target.empty()) {
    return {};
  }
  PjPtr raw(proj_create_crs_to_crs(ctx, source.c_str(), target.c_str(), nullptr));
  if (!raw) {
    return {};
  }
  if (PJ* vis = proj_normalize_for_visualization(ctx, raw.get())) {
    return PjPtr(vis);
  }
  return raw;
}

}  // namespace

struct CoordinateTransform::Impl {
  PjPtr pipeline;
};

CoordinateTransform::CoordinateTransform(std::string_view source_crs,
                                         std::string_view target_crs)
    : impl_(new Impl()) {
  std::lock_guard<std::mutex> lock(g_proj_mu);
  impl_->pipeline =
      make_pipeline_locked(std::string(source_crs), std::string(target_crs));
}

CoordinateTransform::CoordinateTransform(CoordinateTransform&& other) noexcept
    : impl_(other.impl_) {
  other.impl_ = nullptr;
}

CoordinateTransform& CoordinateTransform::operator=(
    CoordinateTransform&& other) noexcept {
  if (this != &other) {
    delete impl_;
    impl_ = other.impl_;
    other.impl_ = nullptr;
  }
  return *this;
}

CoordinateTransform::~CoordinateTransform() {
  delete impl_;
  impl_ = nullptr;
}

bool CoordinateTransform::is_valid() const {
  return impl_ && impl_->pipeline;
}

bool CoordinateTransform::transform_xy(double& x, double& y) const {
  if (!is_valid()) {
    return false;
  }
  std::lock_guard<std::mutex> lock(g_proj_mu);
  PJ* pipeline = impl_->pipeline.get();
  proj_errno_reset(pipeline);
  const PJ_COORD out = proj_trans(pipeline, PJ_FWD, proj_coord(x, y, 0.0, 0.0));
  if (proj_errno(pipeline) != 0 || !std::isfinite(out.xy.x) ||
      !std::isfinite(out.xy.y)) {
    return false;
  }
  x = out.xy.x;
  y = out.xy.y;
  return true;
}

bool transform_xy(std::string_view source_crs,
                  std::string_view target_crs,
                  double& x,
                  double& y) {
  const CoordinateTransform pipeline(source_crs, target_crs);
  return pipeline.transform_xy(x, y);
}

}  // namespace geo
