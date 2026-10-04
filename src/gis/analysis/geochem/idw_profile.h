// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_GEOCHEM_IDW_PROFILE_H_
#define GIS_ANALYSIS_GEOCHEM_IDW_PROFILE_H_

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace gis {
namespace detail {

// One IDW grid fill snapshot (serial vs CPU parallel_for). Local to geochem/.
struct GeochemIdwProfile {
  int width = 0;
  int height = 0;
  int n_pts = 0;
  long long work = 0;
  double dispatch_ms = 0;
  double compute_ms = 0;
  const char* backend = "serial";
};

inline bool geochem_idw_profile_enabled() {
  const char* e = std::getenv("SMT_ANALYSIS_PROFILE");
  if (!e || !e[0]) {
    return false;
  }
  return std::strcmp(e, "1") == 0 || std::strcmp(e, "true") == 0 ||
         std::strcmp(e, "TRUE") == 0;
}

inline double geochem_idw_elapsed_ms(std::chrono::steady_clock::time_point a,
                                     std::chrono::steady_clock::time_point b) {
  return std::chrono::duration<double, std::milli>(b - a).count();
}

inline void emit_geochem_idw_profile(const GeochemIdwProfile& p) {
  if (!geochem_idw_profile_enabled()) {
    return;
  }
  std::fprintf(stderr,
               "[geochem_idw] W=%d H=%d N_pts=%d work=%lld dispatch_ms=%.3f "
               "compute_ms=%.3f backend=%s\n",
               p.width, p.height, p.n_pts, p.work, p.dispatch_ms, p.compute_ms,
               p.backend ? p.backend : "serial");
}

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_GEOCHEM_IDW_PROFILE_H_
