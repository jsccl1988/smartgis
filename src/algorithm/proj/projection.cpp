// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "algorithm/proj/projection.h"

#include "algorithm/proj/proj_backend_traits.h"
#include "algorithm/proj/proj_runtime.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace base;

namespace geo {
namespace {

constexpr const char kWgs84Longlat[] = "EPSG:4326";

void apply_geotransform(const double* gt, dbfPoint* point) {
  const double x_out = gt[0] + gt[1] * point->x + gt[2] * point->y;
  const double y_out = gt[3] + gt[4] * point->x + gt[5] * point->y;
  point->x = x_out;
  point->y = y_out;
}

void free_args(Projection* p) {
  if (!p || !p->args) {
    return;
  }
  for (int i = 0; i < p->numargs; ++i) {
    std::free(p->args[i]);
    p->args[i] = nullptr;
  }
  std::free(p->args);
  p->args = nullptr;
  p->numargs = 0;
}

void destroy_crs(Projection* p) {
  if (!p || !p->proj) {
    return;
  }
  PJ* crs = static_cast<PJ*>(p->proj);
  proj::detail::forget_pj(crs);
  proj_destroy(crs);
  p->proj = nullptr;
}

bool looks_like_authority(const char* value) {
  if (!value || !value[0]) {
    return false;
  }
  if (_strnicmp(value, "EPSG:", 5) == 0) {
    return true;
  }
  if (_strnicmp(value, "ESRI:", 5) == 0) {
    return true;
  }
  if (_strnicmp(value, "IAU_2015:", 9) == 0) {
    return true;
  }
  return false;
}

std::string strip_plus(const char* token) {
  if (!token) {
    return {};
  }
  if (token[0] == '+') {
    return token + 1;
  }
  return token;
}

std::string canonicalize_definition(const char* value) {
  if (!value || !value[0]) {
    return {};
  }
  std::string text = value;
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
    text.erase(text.begin());
  }
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
    text.pop_back();
  }
  if (text.size() >= 11 && _strnicmp(text.c_str(), "+init=epsg:", 11) == 0) {
    return std::string("EPSG:") + (text.c_str() + 11);
  }
  if (text.size() >= 10 && _strnicmp(text.c_str(), "init=epsg:", 10) == 0) {
    return std::string("EPSG:") + (text.c_str() + 10);
  }
  if (looks_like_authority(text.c_str())) {
    if (text[0] == '+') {
      text.erase(text.begin());
    }
    return text;
  }
  return text;
}

bool needs_type_crs(const std::string& definition) {
  if (definition.find("EPSG:") == 0 || definition.find("epsg:") == 0) {
    return false;
  }
  if (definition.find("PROJCS") != std::string::npos ||
      definition.find("GEOGCS") != std::string::npos ||
      definition.find("COMPD_CS") != std::string::npos) {
    return false;
  }
  if (definition.find("+type=crs") != std::string::npos ||
      definition.find("type=crs") != std::string::npos) {
    return false;
  }
  return definition.find("+proj=") != std::string::npos ||
         definition.find("proj=") != std::string::npos;
}

std::string joined_definition(const Projection* p) {
  if (!p || p->numargs <= 0) {
    return {};
  }
  if (p->numargs == 1) {
    return canonicalize_definition(p->args[0]);
  }

  std::string out;
  for (int i = 0; i < p->numargs; ++i) {
    if (!p->args[i] || !p->args[i][0]) {
      continue;
    }
    std::string token = strip_plus(p->args[i]);
    if (token.empty()) {
      continue;
    }
    if (!out.empty()) {
      out += ' ';
    }
    out += '+';
    out += token;
  }
  return canonicalize_definition(out.c_str());
}

std::string crs_definition(const Projection* p) {
  std::string def = joined_definition(p);
  if (def.empty()) {
    return {};
  }
  if (needs_type_crs(def)) {
    def += " +type=crs";
  }
  return def;
}

int replace_args(Projection* p, const std::vector<std::string>& tokens) {
  free_args(p);
  if (tokens.empty()) {
    return SMT_ERR_NONE;
  }
  p->args = static_cast<char**>(std::calloc(tokens.size(), sizeof(char*)));
  if (!p->args) {
    return SMT_ERR_NOT_ENOUGH_MEM;
  }
  p->numargs = static_cast<int>(tokens.size());
  for (size_t i = 0; i < tokens.size(); ++i) {
    p->args[i] = _strdup(tokens[i].c_str());
    if (!p->args[i]) {
      free_args(p);
      return SMT_ERR_NOT_ENOUGH_MEM;
    }
  }
  return SMT_ERR_NONE;
}

std::vector<std::string> tokenize_definition(const char* value) {
  std::vector<std::string> tokens;
  if (!value) {
    return tokens;
  }
  const std::string canon = canonicalize_definition(value);
  if (looks_like_authority(canon.c_str()) ||
      canon.find("PROJCS") != std::string::npos ||
      canon.find("GEOGCS") != std::string::npos) {
    tokens.push_back(canon);
    return tokens;
  }

  const char* cursor = value;
  while (*cursor) {
    while (*cursor && std::isspace(static_cast<unsigned char>(*cursor))) {
      ++cursor;
    }
    if (!*cursor) {
      break;
    }
    const char* start = cursor;
    while (*cursor && !std::isspace(static_cast<unsigned char>(*cursor))) {
      ++cursor;
    }
    tokens.emplace_back(start, cursor);
  }
  return tokens;
}

PJ* projection_crs(const Projection* p) {
  if (!p) {
    return nullptr;
  }
  return static_cast<PJ*>(p->proj);
}

int project_with_defs(const char* source_def,
                      const char* target_def,
                      dbfPoint* point) {
  return proj::proj_backend_traits<dbfPoint>::transform(source_def, target_def,
                                                        *point);
}

}  // namespace

int project_point(Projection* in, Projection* out, dbfPoint* point) {
  if (!point) {
    return SMT_ERR_INVALID_PARAM;
  }

  if (in && in->gt.need_geotransform) {
    apply_geotransform(in->gt.geotransform, point);
  }

  const std::string in_def = crs_definition(in);
  const std::string out_def = crs_definition(out);
  PJ* in_crs = projection_crs(in);
  PJ* out_crs = projection_crs(out);

  const bool same_args = in && out && in->numargs == 1 && out->numargs == 1 &&
                         in->args && out->args && in->args[0] && out->args[0] &&
                         std::strcmp(in->args[0], out->args[0]) == 0;
  const bool same_def = !in_def.empty() && in_def == out_def;

  int rc = SMT_ERR_NONE;
  if (same_args || same_def) {
    rc = SMT_ERR_NONE;
  } else if (in_crs && out_crs) {
    PJ* pipeline = proj::detail::cached_pipeline_from_pj(in_crs, out_crs);
    if (!pipeline) {
      return SMT_ERR_FAILURE;
    }
    rc = proj::proj_backend_traits<dbfPoint>::transform(pipeline, *point,
                                                        PJ_FWD);
  } else if (!in_def.empty() && !out_def.empty()) {
    rc = project_with_defs(in_def.c_str(), out_def.c_str(), point);
  } else if (in_def.empty() && !out_def.empty()) {
    if (proj::detail::is_geographic_crs(out_crs)) {
      rc = SMT_ERR_NONE;
    } else {
      rc = project_with_defs(kWgs84Longlat, out_def.c_str(), point);
    }
  } else if (!in_def.empty() && out_def.empty()) {
    if (proj::detail::is_geographic_crs(in_crs)) {
      rc = SMT_ERR_NONE;
    } else {
      rc = project_with_defs(in_def.c_str(), kWgs84Longlat, point);
    }
  } else {
    rc = SMT_ERR_NONE;
  }

  if (rc != SMT_ERR_NONE) {
    return rc;
  }

  if (out && out->gt.need_geotransform) {
    apply_geotransform(out->gt.invgeotransform, point);
  }
  return SMT_ERR_NONE;
}

int project_rect(Projection* in, Projection* out, dbfRect* rect) {
  if (!rect) {
    return SMT_ERR_INVALID_PARAM;
  }
  if (project_point(in, out, &rect->lb) != SMT_ERR_NONE) {
    return SMT_ERR_FAILURE;
  }
  if (project_point(in, out, &rect->rt) != SMT_ERR_NONE) {
    return SMT_ERR_FAILURE;
  }
  return SMT_ERR_NONE;
}

int projections_differ(Projection* proj1, Projection* proj2) {
  if (!proj1 || !proj2) {
    return 0;
  }
  if (proj1->numargs == 0 || proj2->numargs == 0) {
    return 0;
  }
  if (proj1->numargs != proj2->numargs) {
    return 1;
  }
  if (proj1->gt.need_geotransform || proj2->gt.need_geotransform) {
    return 1;
  }
  for (int i = 0; i < proj1->numargs; ++i) {
    const char* a = proj1->args ? proj1->args[i] : nullptr;
    const char* b = proj2->args ? proj2->args[i] : nullptr;
    if ((a == nullptr) != (b == nullptr)) {
      return 1;
    }
    if (a && std::strcmp(a, b) != 0) {
      return 1;
    }
  }
  return 0;
}

void free_projection(Projection* p) {
  if (!p) {
    return;
  }
  destroy_crs(p);
  free_args(p);
  p->automatic = 0;
  std::memset(&p->gt, 0, sizeof(p->gt));
}

int init_projection(Projection* p) {
  if (!p) {
    return SMT_ERR_INVALID_PARAM;
  }
  free_projection(p);
  std::memset(p, 0, sizeof(*p));
  return SMT_ERR_NONE;
}

int process_projection(Projection* p) {
  if (!p) {
    return SMT_ERR_INVALID_PARAM;
  }
  destroy_crs(p);
  const std::string def = crs_definition(p);
  if (def.empty()) {
    return SMT_ERR_NONE;
  }
  p->proj = proj::detail::create_crs(def.c_str());
  if (!p->proj) {
    return SMT_ERR_FAILURE;
  }
  return SMT_ERR_NONE;
}

int load_projection_string(Projection* p, const char* value) {
  if (!p) {
    return SMT_ERR_INVALID_PARAM;
  }
  if (!value || canonicalize_definition(value).empty()) {
    return SMT_ERR_INVALID_PARAM;
  }
  const int rc = replace_args(p, tokenize_definition(value));
  if (rc != SMT_ERR_NONE) {
    return rc;
  }
  return process_projection(p);
}

double gauss_kruger_central_meridian(double lon_deg) {
  return proj::detail::gauss_kruger_central_meridian(lon_deg);
}

int load_longlat_ellipsoid(Projection* p, double a, double b) {
  const std::string def = proj::detail::make_longlat_crs(a, b);
  return load_projection_string(p, def.c_str());
}

int load_tmerc_crs(Projection* p, double a, double b, double lon_0) {
  const std::string def = proj::detail::make_tmerc_crs(a, b, lon_0);
  return load_projection_string(p, def.c_str());
}

int load_lcc_crs(Projection* p,
                  double a,
                  double b,
                  double lat_1,
                  double lat_2,
                  double lat_0,
                  double lon_0) {
  const std::string def =
      proj::detail::make_lcc_crs(a, b, lat_1, lat_2, lat_0, lon_0);
  return load_projection_string(p, def.c_str());
}

int load_projection_string_epsg(Projection* p, const char* value) {
  if (!p || !value || !value[0]) {
    return SMT_ERR_INVALID_PARAM;
  }
  std::string code = canonicalize_definition(value);
  if (_strnicmp(code.c_str(), "EPSG:", 5) != 0) {
    code = std::string("EPSG:") + value;
  }
  return load_projection_string(p, code.c_str());
}

char* get_projection_string(Projection* proj) {
  if (!proj) {
    return nullptr;
  }
  int n_len = 0;
  for (int i = 0; i < proj->numargs; ++i) {
    if (proj->args && proj->args[i]) {
      n_len += static_cast<int>(std::strlen(proj->args[i]) + 2);
    }
  }
  char* psz_proj_string = static_cast<char*>(std::malloc(n_len + 1));
  if (!psz_proj_string) {
    return nullptr;
  }
  psz_proj_string[0] = '\0';
  for (int i = 0; i < proj->numargs; ++i) {
    if (!proj->args || !proj->args[i] || proj->args[i][0] == '\0') {
      continue;
    }
    if (psz_proj_string[0] == '\0') {
      if (proj->args[i][0] != '+') {
        std::strcat(psz_proj_string, "+");
      }
    } else {
      if (proj->args[i][0] != '+') {
        std::strcat(psz_proj_string, " +");
      } else {
        std::strcat(psz_proj_string, " ");
      }
    }
    std::strcat(psz_proj_string, proj->args[i]);
  }
  return psz_proj_string;
}

void axis_normalize_points(Projection* proj, int count, double* x, double* y) {
  if (!proj || !x || !y) {
    return;
  }
  const char* axis = nullptr;
  for (int i = 0; i < proj->numargs; ++i) {
    if (proj->args && proj->args[i] && std::strstr(proj->args[i], "epsgaxis=")) {
      axis = std::strstr(proj->args[i], "=") + 1;
      break;
    }
  }
  if (!axis || _stricmp(axis, "en") == 0) {
    return;
  }
  if (_stricmp(axis, "ne") != 0) {
    return;
  }
  for (int i = 0; i < count; ++i) {
    const double tmp = x[i];
    x[i] = y[i];
    y[i] = tmp;
  }
}

void axis_denormalize_points(Projection* proj,
                              int count,
                              double* x,
                              double* y) {
  axis_normalize_points(proj, count, x, y);
}

void set_proj_lib(const char* proj_lib, const char* psz_rel_to_path) {
  proj::detail::set_search_path(proj_lib, psz_rel_to_path);
}

}  // namespace geo
