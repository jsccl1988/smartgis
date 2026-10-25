// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/geometry/fit.h"

#include "gis/geo/ops/shape.h"

#include <cmath>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <Eigen/Dense>

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "ogr_api.h"
#include "ogr_geometry.h"
#include "ogrsf_frmts.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace gis {
namespace detail {
namespace {

bool parse_args(std::string_view json, rapidjson::Document* out) {
  if (!out || json.empty()) {
    return false;
  }
  out->Parse(json.data(), static_cast<rapidjson::SizeType>(json.size()));
  return !out->HasParseError() && out->IsObject();
}

bool json_get_string(const rapidjson::Value& obj,
                     const char* key,
                     std::string* out) {
  if (!out || !key || !obj.IsObject()) {
    return false;
  }
  const auto it = obj.FindMember(key);
  if (it == obj.MemberEnd() || !it->value.IsString()) {
    return false;
  }
  *out = std::string(it->value.GetString(), it->value.GetStringLength());
  return !out->empty();
}

void collect_points_2d(OGRGeometry* geom, std::vector<double>* xy) {
  if (!geom || !xy) {
    return;
  }
  const OGRwkbGeometryType t = wkbFlatten(geom->getGeometryType());
  if (t == wkbPoint) {
    auto* p = geom->toPoint();
    xy->push_back(p->getX());
    xy->push_back(p->getY());
    return;
  }
  if (t == wkbMultiPoint) {
    auto* mp = geom->toMultiPoint();
    for (int i = 0; i < mp->getNumGeometries(); ++i) {
      collect_points_2d(mp->getGeometryRef(i), xy);
    }
    return;
  }
  if (t == wkbLineString) {
    auto* line = geom->toLineString();
    for (int i = 0; i < line->getNumPoints(); ++i) {
      xy->push_back(line->getX(i));
      xy->push_back(line->getY(i));
    }
    return;
  }
  if (t == wkbGeometryCollection || t == wkbMultiLineString ||
      t == wkbMultiPolygon || t == wkbPolygon) {
    OGRGeometryCollection* coll = dynamic_cast<OGRGeometryCollection*>(geom);
    if (!coll) {
      return;
    }
    for (int i = 0; i < coll->getNumGeometries(); ++i) {
      collect_points_2d(coll->getGeometryRef(i), xy);
    }
  }
}

void collect_points_3d(OGRGeometry* geom, std::vector<double>* xyz) {
  if (!geom || !xyz) {
    return;
  }
  const OGRwkbGeometryType t = wkbFlatten(geom->getGeometryType());
  if (t == wkbPoint) {
    auto* p = geom->toPoint();
    xyz->push_back(p->getX());
    xyz->push_back(p->getY());
    xyz->push_back(p->getZ());
    return;
  }
  if (t == wkbMultiPoint) {
    auto* mp = geom->toMultiPoint();
    for (int i = 0; i < mp->getNumGeometries(); ++i) {
      collect_points_3d(mp->getGeometryRef(i), xyz);
    }
    return;
  }
  if (t == wkbLineString) {
    auto* line = geom->toLineString();
    for (int i = 0; i < line->getNumPoints(); ++i) {
      xyz->push_back(line->getX(i));
      xyz->push_back(line->getY(i));
      xyz->push_back(line->getZ(i));
    }
  }
}

std::unique_ptr<OGRGeometry> load_first_geometry(const std::string& path) {
  GDALAllRegister();
  GDALDatasetUniquePtr ds(static_cast<GDALDataset*>(GDALOpenEx(
      path.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, nullptr, nullptr,
      nullptr)));
  if (!ds) {
    return nullptr;
  }
  for (int i = 0; i < ds->GetLayerCount(); ++i) {
    OGRLayer* layer = ds->GetLayer(i);
    if (!layer) {
      continue;
    }
    layer->ResetReading();
    while (OGRFeature* feat = layer->GetNextFeature()) {
      OGRFeatureUniquePtr holder(feat);
      OGRGeometry* g = feat->GetGeometryRef();
      if (g && !g->IsEmpty()) {
        return std::unique_ptr<OGRGeometry>(g->clone());
      }
    }
  }
  return nullptr;
}

bool write_geometry_geojson(const std::string& path, OGRGeometry* geom) {
  if (!geom || path.empty()) {
    return false;
  }
  GDALAllRegister();
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GeoJSON");
  if (!driver) {
    return false;
  }
  VSIUnlink(path.c_str());
  GDALDatasetUniquePtr ds(
      driver->Create(path.c_str(), 0, 0, 0, GDT_Unknown, nullptr));
  if (!ds) {
    return false;
  }
  OGRLayer* layer = ds->CreateLayer("result", nullptr, wkbUnknown, nullptr);
  if (!layer) {
    return false;
  }
  OGRFeatureDefn* defn = layer->GetLayerDefn();
  OGRFeatureUniquePtr feat(OGRFeature::CreateFeature(defn));
  if (!feat) {
    return false;
  }
  feat->SetGeometry(geom);
  return layer->CreateFeature(feat.get()) == OGRERR_NONE;
}

bool write_text_file(const std::string& path, const std::string& body) {
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    return false;
  }
  out.write(body.data(), static_cast<std::streamsize>(body.size()));
  return static_cast<bool>(out);
}

}  // namespace

FitLineResult fit_line_2d(const std::vector<double>& xy) {
  FitLineResult out;
  const size_t n = xy.size() / 2;
  if (xy.size() % 2 != 0 || n < 2) {
    out.error = "need_at_least_2_points";
    return out;
  }
  Eigen::MatrixXd pts(static_cast<Eigen::Index>(n), 2);
  for (size_t i = 0; i < n; ++i) {
    pts(static_cast<Eigen::Index>(i), 0) = xy[2 * i];
    pts(static_cast<Eigen::Index>(i), 1) = xy[2 * i + 1];
  }
  const Eigen::RowVector2d mean = pts.colwise().mean();
  out.cx = mean(0);
  out.cy = mean(1);
  const Eigen::MatrixXd centered = pts.rowwise() - mean;
  Eigen::JacobiSVD<Eigen::MatrixXd> svd(
      centered, Eigen::ComputeThinU | Eigen::ComputeThinV);
  const Eigen::Vector2d dir = svd.matrixV().col(0);
  out.dx = dir(0);
  out.dy = dir(1);
  const double nrm = std::hypot(out.dx, out.dy);
  if (nrm < 1e-18) {
    out.error = "degenerate";
    return out;
  }
  out.dx /= nrm;
  out.dy /= nrm;
  double sum_sq = 0.0;
  for (size_t i = 0; i < n; ++i) {
    const double vx = xy[2 * i] - out.cx;
    const double vy = xy[2 * i + 1] - out.cy;
    const double proj = vx * out.dx + vy * out.dy;
    const double rx = vx - proj * out.dx;
    const double ry = vy - proj * out.dy;
    sum_sq += rx * rx + ry * ry;
  }
  out.rms = std::sqrt(sum_sq / static_cast<double>(n));
  out.ok = true;
  return out;
}

FitPlaneResult fit_plane_3d(const std::vector<double>& xyz) {
  FitPlaneResult out;
  const size_t n = xyz.size() / 3;
  if (xyz.size() % 3 != 0 || n < 3) {
    out.error = "need_at_least_3_points";
    return out;
  }
  Eigen::MatrixXd pts(static_cast<Eigen::Index>(n), 3);
  for (size_t i = 0; i < n; ++i) {
    pts(static_cast<Eigen::Index>(i), 0) = xyz[3 * i];
    pts(static_cast<Eigen::Index>(i), 1) = xyz[3 * i + 1];
    pts(static_cast<Eigen::Index>(i), 2) = xyz[3 * i + 2];
  }
  const Eigen::RowVector3d mean = pts.colwise().mean();
  const Eigen::MatrixXd centered = pts.rowwise() - mean;
  Eigen::JacobiSVD<Eigen::MatrixXd> svd(
      centered, Eigen::ComputeThinU | Eigen::ComputeThinV);
  const Eigen::Vector3d normal = svd.matrixV().col(2);
  const double nrm = normal.norm();
  if (nrm < 1e-18) {
    out.error = "degenerate";
    return out;
  }
  out.a = normal(0) / nrm;
  out.b = normal(1) / nrm;
  out.c = normal(2) / nrm;
  out.d = -(out.a * mean(0) + out.b * mean(1) + out.c * mean(2));
  double sum_sq = 0.0;
  for (size_t i = 0; i < n; ++i) {
    const double r =
        out.a * xyz[3 * i] + out.b * xyz[3 * i + 1] + out.c * xyz[3 * i + 2] +
        out.d;
    sum_sq += r * r;
  }
  out.rms = std::sqrt(sum_sq / static_cast<double>(n));
  out.ok = true;
  return out;
}

FitLineResult fit_line_2d_from_ogr(const OGRGeometry& geom) {
  std::vector<double> xy;
  collect_points_2d(const_cast<OGRGeometry*>(&geom), &xy);
  return fit_line_2d(xy);
}

FitPlaneResult fit_plane_3d_from_ogr(const OGRGeometry& geom) {
  std::vector<double> xyz;
  collect_points_3d(const_cast<OGRGeometry*>(&geom), &xyz);
  return fit_plane_3d(xyz);
}

AffineAlignResult affine_align_2d(const std::vector<double>& src_xy,
                                  const std::vector<double>& dst_xy) {
  AffineAlignResult out;
  const size_t n = src_xy.size() / 2;
  if (src_xy.size() % 2 != 0 || dst_xy.size() != src_xy.size() || n < 3) {
    out.error = "need_at_least_3_paired_points";
    return out;
  }
  // Solve [x y 1 0 0 0; 0 0 0 x y 1] * m = [x'; y'] for each pair.
  Eigen::MatrixXd A(static_cast<Eigen::Index>(2 * n), 6);
  Eigen::VectorXd b(static_cast<Eigen::Index>(2 * n));
  A.setZero();
  for (size_t i = 0; i < n; ++i) {
    const double x = src_xy[2 * i];
    const double y = src_xy[2 * i + 1];
    const Eigen::Index r0 = static_cast<Eigen::Index>(2 * i);
    const Eigen::Index r1 = r0 + 1;
    A(r0, 0) = x;
    A(r0, 1) = y;
    A(r0, 2) = 1.0;
    A(r1, 3) = x;
    A(r1, 4) = y;
    A(r1, 5) = 1.0;
    b(r0) = dst_xy[2 * i];
    b(r1) = dst_xy[2 * i + 1];
  }
  Eigen::ColPivHouseholderQR<Eigen::MatrixXd> qr(A);
  if (qr.rank() < 6) {
    out.error = "rank_deficient";
    return out;
  }
  const Eigen::VectorXd m = qr.solve(b);
  for (int i = 0; i < 6; ++i) {
    out.m[i] = m(i);
  }
  double sum_sq = 0.0;
  for (size_t i = 0; i < n; ++i) {
    const double x = src_xy[2 * i];
    const double y = src_xy[2 * i + 1];
    const double xp = out.m[0] * x + out.m[1] * y + out.m[2];
    const double yp = out.m[3] * x + out.m[4] * y + out.m[5];
    const double dx = xp - dst_xy[2 * i];
    const double dy = yp - dst_xy[2 * i + 1];
    sum_sq += dx * dx + dy * dy;
  }
  out.rms = std::sqrt(sum_sq / static_cast<double>(n));
  out.ok = true;
  return out;
}

namespace {

void similarity_to_affine_matrix(double scale,
                                double theta,
                                double tx,
                                double ty,
                                double m[6]) {
  const double c = std::cos(theta);
  const double s = std::sin(theta);
  m[0] = scale * c;
  m[1] = -scale * s;
  m[2] = tx;
  m[3] = scale * s;
  m[4] = scale * c;
  m[5] = ty;
}

double align_rms(const std::vector<double>& src_xy,
                 const std::vector<double>& dst_xy,
                 const double m[6]) {
  const size_t n = src_xy.size() / 2;
  double sum_sq = 0.0;
  for (size_t i = 0; i < n; ++i) {
    const double x = src_xy[2 * i];
    const double y = src_xy[2 * i + 1];
    const double xp = m[0] * x + m[1] * y + m[2];
    const double yp = m[3] * x + m[4] * y + m[5];
    const double dx = xp - dst_xy[2 * i];
    const double dy = yp - dst_xy[2 * i + 1];
    sum_sq += dx * dx + dy * dy;
  }
  return std::sqrt(sum_sq / static_cast<double>(n));
}

SimilarityAlignResult umeyama_align_2d(const std::vector<double>& src_xy,
                                       const std::vector<double>& dst_xy,
                                       bool rigid) {
  SimilarityAlignResult out;
  const size_t n = src_xy.size() / 2;
  if (src_xy.size() % 2 != 0 || dst_xy.size() != src_xy.size() || n < 2) {
    out.error = "need_at_least_2_paired_points";
    return out;
  }
  Eigen::MatrixXd src(static_cast<Eigen::Index>(n), 2);
  Eigen::MatrixXd dst(static_cast<Eigen::Index>(n), 2);
  for (size_t i = 0; i < n; ++i) {
    src(static_cast<Eigen::Index>(i), 0) = src_xy[2 * i];
    src(static_cast<Eigen::Index>(i), 1) = src_xy[2 * i + 1];
    dst(static_cast<Eigen::Index>(i), 0) = dst_xy[2 * i];
    dst(static_cast<Eigen::Index>(i), 1) = dst_xy[2 * i + 1];
  }
  const Eigen::RowVector2d mu_src = src.colwise().mean();
  const Eigen::RowVector2d mu_dst = dst.colwise().mean();
  const Eigen::MatrixXd src_c = src.rowwise() - mu_src;
  const Eigen::MatrixXd dst_c = dst.rowwise() - mu_dst;
  const Eigen::Matrix2d cov =
      (src_c.transpose() * dst_c) / static_cast<double>(n);
  Eigen::JacobiSVD<Eigen::Matrix2d> svd(
      cov, Eigen::ComputeFullU | Eigen::ComputeFullV);
  Eigen::Matrix2d R = svd.matrixV() * svd.matrixU().transpose();
  if (R.determinant() < 0.0) {
    Eigen::Matrix2d v = svd.matrixV();
    v.col(1) *= -1.0;
    R = v * svd.matrixU().transpose();
  }
  const double var_src =
      src_c.array().square().sum() / static_cast<double>(n);
  if (var_src < 1e-24) {
    out.error = "degenerate";
    return out;
  }
  double scale = 1.0;
  if (!rigid) {
    scale = svd.singularValues().sum() / var_src;
  }
  const Eigen::Vector2d t =
      mu_dst.transpose() - scale * R * mu_src.transpose();
  out.scale = scale;
  out.theta = std::atan2(R(1, 0), R(0, 0));
  out.tx = t(0);
  out.ty = t(1);
  similarity_to_affine_matrix(scale, out.theta, out.tx, out.ty, out.m);
  out.rms = align_rms(src_xy, dst_xy, out.m);
  out.ok = true;
  return out;
}

}  // namespace

SimilarityAlignResult similarity_align_2d(const std::vector<double>& src_xy,
                                            const std::vector<double>& dst_xy) {
  return umeyama_align_2d(src_xy, dst_xy, false);
}

SimilarityAlignResult rigid_align_2d(const std::vector<double>& src_xy,
                                     const std::vector<double>& dst_xy) {
  return umeyama_align_2d(src_xy, dst_xy, true);
}

PcaAxesResult pca_axes_2d(const std::vector<double>& xy) {
  PcaAxesResult out;
  const size_t n = xy.size() / 2;
  if (xy.size() % 2 != 0 || n < 2) {
    out.error = "need_at_least_2_points";
    return out;
  }
  Eigen::MatrixXd pts(static_cast<Eigen::Index>(n), 2);
  for (size_t i = 0; i < n; ++i) {
    pts(static_cast<Eigen::Index>(i), 0) = xy[2 * i];
    pts(static_cast<Eigen::Index>(i), 1) = xy[2 * i + 1];
  }
  const Eigen::RowVector2d mean = pts.colwise().mean();
  out.cx = mean(0);
  out.cy = mean(1);
  const Eigen::MatrixXd centered = pts.rowwise() - mean;
  const Eigen::Matrix2d cov =
      (centered.transpose() * centered) / static_cast<double>(n - 1);
  Eigen::SelfAdjointEigenSolver<Eigen::Matrix2d> es(cov);
  if (es.info() != Eigen::Success) {
    out.error = "eigen_failed";
    return out;
  }
  Eigen::Vector2d evals = es.eigenvalues();
  Eigen::Matrix2d evecs = es.eigenvectors();
  int major_idx = 0;
  if (evals(1) > evals(0)) {
    major_idx = 1;
  }
  const int minor_idx = 1 - major_idx;
  out.eigenvalue_major = evals(major_idx);
  out.eigenvalue_minor = evals(minor_idx);
  Eigen::Vector2d major = evecs.col(major_idx);
  Eigen::Vector2d minor = evecs.col(minor_idx);
  const double major_nrm = major.norm();
  const double minor_nrm = minor.norm();
  if (major_nrm < 1e-18 || minor_nrm < 1e-18) {
    out.error = "degenerate";
    return out;
  }
  out.major_dx = major(0) / major_nrm;
  out.major_dy = major(1) / major_nrm;
  out.minor_dx = minor(0) / minor_nrm;
  out.minor_dy = minor(1) / minor_nrm;
  double min_major = 0.0;
  double max_major = 0.0;
  double min_minor = 0.0;
  double max_minor = 0.0;
  for (size_t i = 0; i < n; ++i) {
    const double vx = xy[2 * i] - out.cx;
    const double vy = xy[2 * i + 1] - out.cy;
    const double t_major = vx * out.major_dx + vy * out.major_dy;
    const double t_minor = vx * out.minor_dx + vy * out.minor_dy;
    if (i == 0 || t_major < min_major) {
      min_major = t_major;
    }
    if (i == 0 || t_major > max_major) {
      max_major = t_major;
    }
    if (i == 0 || t_minor < min_minor) {
      min_minor = t_minor;
    }
    if (i == 0 || t_minor > max_minor) {
      max_minor = t_minor;
    }
  }
  out.major_extent = max_major - min_major;
  out.minor_extent = max_minor - min_minor;
  out.ok = true;
  return out;
}

PcaAxesResult pca_axes_2d_from_ogr(const OGRGeometry& geom) {
  std::vector<double> xy;
  collect_points_2d(const_cast<OGRGeometry*>(&geom), &xy);
  return pca_axes_2d(xy);
}

std::unique_ptr<OGRGeometry> oriented_bbox_from_points_2d(
    const std::vector<double>& xy) {
  const PcaAxesResult pca = pca_axes_2d(xy);
  if (!pca.ok) {
    return nullptr;
  }
  const size_t n = xy.size() / 2;
  double min_major = 0.0;
  double max_major = 0.0;
  double min_minor = 0.0;
  double max_minor = 0.0;
  for (size_t i = 0; i < n; ++i) {
    const double vx = xy[2 * i] - pca.cx;
    const double vy = xy[2 * i + 1] - pca.cy;
    const double t_major = vx * pca.major_dx + vy * pca.major_dy;
    const double t_minor = vx * pca.minor_dx + vy * pca.minor_dy;
    if (i == 0 || t_major < min_major) {
      min_major = t_major;
    }
    if (i == 0 || t_major > max_major) {
      max_major = t_major;
    }
    if (i == 0 || t_minor < min_minor) {
      min_minor = t_minor;
    }
    if (i == 0 || t_minor > max_minor) {
      max_minor = t_minor;
    }
  }
  auto ring = std::make_unique<OGRLinearRing>();
  auto corner = [&](double tm, double tn) {
    const double x =
        pca.cx + tm * pca.major_dx + tn * pca.minor_dx;
    const double y =
        pca.cy + tm * pca.major_dy + tn * pca.minor_dy;
    ring->addPoint(x, y);
  };
  corner(min_major, min_minor);
  corner(max_major, min_minor);
  corner(max_major, max_minor);
  corner(min_major, max_minor);
  corner(min_major, min_minor);
  auto poly = std::make_unique<OGRPolygon>();
  poly->addRing(ring.release());
  return poly;
}

std::unique_ptr<OGRGeometry> oriented_bbox_from_ogr(const OGRGeometry& geom) {
  std::vector<double> xy;
  collect_points_2d(const_cast<OGRGeometry*>(&geom), &xy);
  if (xy.size() >= 4) {
    if (auto pca_box = oriented_bbox_from_points_2d(xy)) {
      return pca_box;
    }
  }
  OGRGeometry* mrr = geo::minimum_rotated_rectangle(geom);
  if (!mrr) {
    return nullptr;
  }
  return std::unique_ptr<OGRGeometry>(mrr);
}

bool run_fit_line_op(std::string_view args_json) {
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    return false;
  }
  std::string input;
  std::string output;
  if (!json_get_string(args, "input", &input) ||
      !json_get_string(args, "output", &output)) {
    return false;
  }
  auto geom = load_first_geometry(input);
  if (!geom) {
    return false;
  }
  std::vector<double> xy;
  collect_points_2d(geom.get(), &xy);
  const FitLineResult fit = fit_line_2d(xy);
  if (!fit.ok) {
    return false;
  }
  double t_min = 0.0;
  double t_max = 0.0;
  for (size_t i = 0; i < xy.size() / 2; ++i) {
    const double t =
        (xy[2 * i] - fit.cx) * fit.dx + (xy[2 * i + 1] - fit.cy) * fit.dy;
    if (i == 0 || t < t_min) {
      t_min = t;
    }
    if (i == 0 || t > t_max) {
      t_max = t;
    }
  }
  OGRLineString line;
  line.addPoint(fit.cx + t_min * fit.dx, fit.cy + t_min * fit.dy);
  line.addPoint(fit.cx + t_max * fit.dx, fit.cy + t_max * fit.dy);
  return write_geometry_geojson(output, &line);
}

bool run_fit_plane_op(std::string_view args_json) {
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    return false;
  }
  std::string input;
  std::string output;
  if (!json_get_string(args, "input", &input) ||
      !json_get_string(args, "output", &output)) {
    return false;
  }
  auto geom = load_first_geometry(input);
  if (!geom) {
    return false;
  }
  std::vector<double> xyz;
  collect_points_3d(geom.get(), &xyz);
  const FitPlaneResult fit = fit_plane_3d(xyz);
  if (!fit.ok) {
    return false;
  }
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  w.Key("a");
  w.Double(fit.a);
  w.Key("b");
  w.Double(fit.b);
  w.Key("c");
  w.Double(fit.c);
  w.Key("d");
  w.Double(fit.d);
  w.Key("rms");
  w.Double(fit.rms);
  w.EndObject();
  return write_text_file(output, buf.GetString());
}

bool run_affine_align_op(std::string_view args_json) {
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    return false;
  }
  std::string source;
  std::string target;
  std::string output;
  if (!json_get_string(args, "source", &source) ||
      !json_get_string(args, "target", &target) ||
      !json_get_string(args, "output", &output)) {
    return false;
  }
  auto src_g = load_first_geometry(source);
  auto dst_g = load_first_geometry(target);
  if (!src_g || !dst_g) {
    return false;
  }
  std::vector<double> src_xy;
  std::vector<double> dst_xy;
  collect_points_2d(src_g.get(), &src_xy);
  collect_points_2d(dst_g.get(), &dst_xy);
  const AffineAlignResult fit = affine_align_2d(src_xy, dst_xy);
  if (!fit.ok) {
    return false;
  }
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  w.Key("m");
  w.StartArray();
  for (double v : fit.m) {
    w.Double(v);
  }
  w.EndArray();
  w.Key("rms");
  w.Double(fit.rms);
  w.EndObject();
  return write_text_file(output, buf.GetString());
}

bool run_similarity_align_op(std::string_view args_json) {
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    return false;
  }
  std::string source;
  std::string target;
  std::string output;
  if (!json_get_string(args, "source", &source) ||
      !json_get_string(args, "target", &target) ||
      !json_get_string(args, "output", &output)) {
    return false;
  }
  auto src_g = load_first_geometry(source);
  auto dst_g = load_first_geometry(target);
  if (!src_g || !dst_g) {
    return false;
  }
  std::vector<double> src_xy;
  std::vector<double> dst_xy;
  collect_points_2d(src_g.get(), &src_xy);
  collect_points_2d(dst_g.get(), &dst_xy);
  const SimilarityAlignResult fit = similarity_align_2d(src_xy, dst_xy);
  if (!fit.ok) {
    return false;
  }
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  w.Key("scale");
  w.Double(fit.scale);
  w.Key("theta");
  w.Double(fit.theta);
  w.Key("tx");
  w.Double(fit.tx);
  w.Key("ty");
  w.Double(fit.ty);
  w.Key("m");
  w.StartArray();
  for (double v : fit.m) {
    w.Double(v);
  }
  w.EndArray();
  w.Key("rms");
  w.Double(fit.rms);
  w.EndObject();
  return write_text_file(output, buf.GetString());
}

bool run_rigid_align_op(std::string_view args_json) {
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    return false;
  }
  std::string source;
  std::string target;
  std::string output;
  if (!json_get_string(args, "source", &source) ||
      !json_get_string(args, "target", &target) ||
      !json_get_string(args, "output", &output)) {
    return false;
  }
  auto src_g = load_first_geometry(source);
  auto dst_g = load_first_geometry(target);
  if (!src_g || !dst_g) {
    return false;
  }
  std::vector<double> src_xy;
  std::vector<double> dst_xy;
  collect_points_2d(src_g.get(), &src_xy);
  collect_points_2d(dst_g.get(), &dst_xy);
  const SimilarityAlignResult fit = rigid_align_2d(src_xy, dst_xy);
  if (!fit.ok) {
    return false;
  }
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  w.Key("theta");
  w.Double(fit.theta);
  w.Key("tx");
  w.Double(fit.tx);
  w.Key("ty");
  w.Double(fit.ty);
  w.Key("m");
  w.StartArray();
  for (double v : fit.m) {
    w.Double(v);
  }
  w.EndArray();
  w.Key("rms");
  w.Double(fit.rms);
  w.EndObject();
  return write_text_file(output, buf.GetString());
}

bool run_pca_axes_op(std::string_view args_json) {
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    return false;
  }
  std::string input;
  std::string output;
  if (!json_get_string(args, "input", &input) ||
      !json_get_string(args, "output", &output)) {
    return false;
  }
  auto geom = load_first_geometry(input);
  if (!geom) {
    return false;
  }
  const PcaAxesResult pca = pca_axes_2d_from_ogr(*geom);
  if (!pca.ok) {
    return false;
  }
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  w.Key("cx");
  w.Double(pca.cx);
  w.Key("cy");
  w.Double(pca.cy);
  w.Key("major");
  w.StartObject();
  w.Key("dx");
  w.Double(pca.major_dx);
  w.Key("dy");
  w.Double(pca.major_dy);
  w.Key("extent");
  w.Double(pca.major_extent);
  w.Key("eigenvalue");
  w.Double(pca.eigenvalue_major);
  w.EndObject();
  w.Key("minor");
  w.StartObject();
  w.Key("dx");
  w.Double(pca.minor_dx);
  w.Key("dy");
  w.Double(pca.minor_dy);
  w.Key("extent");
  w.Double(pca.minor_extent);
  w.Key("eigenvalue");
  w.Double(pca.eigenvalue_minor);
  w.EndObject();
  w.EndObject();
  return write_text_file(output, buf.GetString());
}

bool run_oriented_bbox_op(std::string_view args_json) {
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    return false;
  }
  std::string input;
  std::string output;
  if (!json_get_string(args, "input", &input) ||
      !json_get_string(args, "output", &output)) {
    return false;
  }
  auto geom = load_first_geometry(input);
  if (!geom) {
    return false;
  }
  auto box = oriented_bbox_from_ogr(*geom);
  if (!box || box->IsEmpty()) {
    return false;
  }
  return write_geometry_geojson(output, box.get());
}

}  // namespace detail
}  // namespace gis
