// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/datasource/gdal/sdbd_handler.h"

#include <sstream>

#include "gdal_priv.h"
#include "ogrsf_frmts.h"
#include "gis/datasource/gdal/gdal_driver.h"
#include "gis/datasource/gdal/sdbd_gdal_driver.h"
#include "gis/datasource/gdal/sdbd_json.h"

namespace gis {
namespace datasource {
namespace {

std::string path_only(const std::string& path) {
  const auto q = path.find('?');
  return q == std::string::npos ? path : path.substr(0, q);
}

std::string ascii_tolower(std::string s) {
  for (char& c : s) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return s;
}

const char* geom_type_name(OGRwkbGeometryType wkb) {
  switch (wkbFlatten(wkb)) {
    case wkbPoint:
      return "point";
    case wkbLineString:
    case wkbMultiLineString:
      return "line";
    case wkbPolygon:
    case wkbMultiPolygon:
      return "polygon";
    case wkbTIN:
      return "tin";
    case wkbNone:
      return "none";
    default:
      return "unknown";
  }
}

OGRwkbGeometryType geom_type_of(const std::string& name) {
  const std::string t = ascii_tolower(name);
  if (t == "point") {
    return wkbPoint;
  }
  if (t == "line" || t == "linestring") {
    return wkbLineString;
  }
  if (t == "polygon") {
    return wkbPolygon;
  }
  if (t == "tin") {
    return wkbTIN;
  }
  return wkbUnknown;
}

OGRFieldType field_type_of(const std::string& name) {
  const std::string t = ascii_tolower(name);
  if (t == "int64" || t == "uint64" || t == "integer" || t == "int") {
    return OFTInteger64;
  }
  if (t == "real" || t == "double") {
    return OFTReal;
  }
  return OFTString;
}

const char* field_type_name(OGRFieldType t) {
  switch (t) {
    case OFTInteger:
    case OFTInteger64:
      return "int64";
    case OFTReal:
      return "double";
    default:
      return "string";
  }
}

std::string layer_crs(OGRLayer* lyr) {
  if (!lyr || !lyr->GetSpatialRef()) {
    return {};
  }
  const char* name = lyr->GetSpatialRef()->GetName();
  return name ? name : "";
}

LayerInfo fill_layer_info(OGRLayer* lyr) {
  LayerInfo info;
  if (!lyr) {
    return info;
  }
  info.name = lyr->GetName() ? lyr->GetName() : "";
  info.crs = layer_crs(lyr);
  info.geom_type = geom_type_name(lyr->GetGeomType());
  const char* geom_col = lyr->GetGeometryColumn();
  info.geom_column = (geom_col && geom_col[0]) ? geom_col : "geom";
  OGREnvelope env;
  if (lyr->GetExtent(&env, TRUE) == OGRERR_NONE) {
    info.extent = {env.MinX, env.MinY, env.MaxX, env.MaxY};
  }
  OGRFeatureDefn* defn = lyr->GetLayerDefn();
  if (defn) {
    for (int i = 0; i < defn->GetFieldCount(); ++i) {
      OGRFieldDefn* fld = defn->GetFieldDefn(i);
      if (!fld) {
        continue;
      }
      FieldInfo fi;
      fi.name = fld->GetNameRef() ? fld->GetNameRef() : "";
      fi.type = field_type_name(fld->GetType());
      if (!fi.name.empty()) {
        info.fields.push_back(std::move(fi));
      }
    }
  }
  return info;
}

std::string make_sdbd_open_name(const Json& json) {
  const std::string driver = json.string("driver");
  const std::string target = json.string("target");
  if (driver.empty() || EQUAL(driver.c_str(), "Memory") ||
      EQUAL(driver.c_str(), "MEM") || EQUAL(driver.c_str(), kSdbdDriverName)) {
    std::string name = kSdbdPrefix;
    name += "MEM:";
    name += target.empty() ? "sdbd" : target;
    return name;
  }
  std::string name = kSdbdPrefix;
  name += driver;
  name += ':';
  name += target;
  return name;
}

}  // namespace

SdbdHandler::SdbdHandler() {
  ready_ = register_gdal_driver() && register_sdbd_driver();
}

SdbdHandler::~SdbdHandler() { close_dataset(); }

void SdbdHandler::close_dataset() {
  cursors_.clear();
  if (dataset_) {
    GDALClose(dataset_);
    dataset_ = nullptr;
  }
}

void SdbdHandler::write_error(int http, const char* code, const char* message,
                              int* status, std::string* response) const {
  if (status) {
    *status = http;
  }
  if (response) {
    *response = std::string("{\"ok\":false,\"error\":\"") + code +
                "\",\"message\":\"" + json_escape(message) + "\"}";
  }
}

void SdbdHandler::write_ok(const std::string& body, int* status,
                           std::string* response) const {
  if (status) {
    *status = 200;
  }
  if (response) {
    *response = body;
  }
}

bool SdbdHandler::parse_open_request(const std::string& body, OpenRequest* req,
                                     int* status, std::string* response) const {
  if (body_has_forbidden_sql(body)) {
    write_error(400, "forbidden_sql", "sql keys are not allowed", status,
                response);
    return false;
  }
  Json json;
  if (!parse_json(body, &json, nullptr) || json.type != Json::kObject) {
    write_error(400, "bad_request", "invalid JSON", status, response);
    return false;
  }
  req->layer = json.string("layer");
  if (req->layer.empty()) {
    write_error(400, "bad_request", "missing layer", status, response);
    return false;
  }
  if (const Json* columns = json.get("columns")) {
    if (columns->type == Json::kArray) {
      for (const auto& c : columns->a) {
        if (c.type == Json::kString) {
          req->columns.push_back(c.s);
        }
      }
    }
  }
  if (const Json* bbox = json.get("bbox")) {
    req->has_bbox = true;
    req->bbox.min_x = bbox->number("min_x");
    req->bbox.min_y = bbox->number("min_y");
    req->bbox.max_x = bbox->number("max_x");
    req->bbox.max_y = bbox->number("max_y");
    if (req->bbox.max_x < req->bbox.min_x ||
        req->bbox.max_y < req->bbox.min_y) {
      write_error(400, "bad_request", "invalid bbox", status, response);
      return false;
    }
  }
  req->predicate = json.string("predicate", "intersects");
  req->crs = json.string("crs");
  if (json.has("limit")) {
    req->limit = static_cast<int>(json.number("limit"));
  }
  if (!req->predicate.empty() && req->predicate != "intersects") {
    write_error(400, "unsupported_predicate", "only intersects is supported",
                status, response);
    return false;
  }
  if (req->limit > kSdbdLimitHardCap) {
    write_error(400, "limit_exceeded", "limit exceeds hard cap", status,
                response);
    return false;
  }
  return true;
}

LayerInfo SdbdHandler::layer_info_of(const std::string& name) const {
  if (!dataset_) {
    return {};
  }
  return fill_layer_info(dataset_->GetLayerByName(name.c_str()));
}

std::vector<LayerInfo> SdbdHandler::catalog() const {
  std::vector<LayerInfo> out;
  if (!dataset_) {
    return out;
  }
  const int n = dataset_->GetLayerCount();
  for (int i = 0; i < n; ++i) {
    LayerInfo info = fill_layer_info(dataset_->GetLayer(i));
    if (!info.name.empty()) {
      out.push_back(std::move(info));
    }
  }
  if (dataset_->GetRasterCount() > 0) {
    LayerInfo ras;
    ras.name =
        dataset_->GetDescription() ? dataset_->GetDescription() : "raster";
    ras.geom_type = "raster";
    ras.geom_column = "";
    out.push_back(std::move(ras));
  }
  return out;
}

std::vector<std::int64_t> SdbdHandler::query_hits(
    const OpenRequest& req) const {
  std::vector<std::int64_t> hits;
  if (!dataset_) {
    return hits;
  }
  OGRLayer* lyr = dataset_->GetLayerByName(req.layer.c_str());
  if (!lyr) {
    return hits;
  }
  if (req.has_bbox) {
    lyr->SetSpatialFilterRect(req.bbox.min_x, req.bbox.min_y, req.bbox.max_x,
                              req.bbox.max_y);
  } else {
    lyr->SetSpatialFilter(nullptr);
  }
  lyr->ResetReading();
  while (OGRFeature* f = lyr->GetNextFeature()) {
    hits.push_back(f->GetFID());
    OGRFeature::DestroyFeature(f);
    if (req.limit > 0 && static_cast<int>(hits.size()) >= req.limit) {
      break;
    }
  }
  lyr->SetSpatialFilter(nullptr);
  return hits;
}

FeatureSet SdbdHandler::features_of(
    const std::string& layer, const std::string& crs, int offset,
    const std::vector<std::int64_t>& fids) const {
  FeatureSet set;
  set.layer = layer;
  set.crs = crs;
  set.offset = offset;
  if (!dataset_) {
    return set;
  }
  OGRLayer* lyr = dataset_->GetLayerByName(layer.c_str());
  if (!lyr) {
    return set;
  }
  OGRFeatureDefn* defn = lyr->GetLayerDefn();
  for (std::int64_t fid : fids) {
    OGRFeature* ogr = lyr->GetFeature(static_cast<GIntBig>(fid));
    if (!ogr) {
      continue;
    }
    Feature feat;
    feat.id = std::to_string(ogr->GetFID());
    if (OGRGeometry* geom = ogr->GetGeometryRef()) {
      char* wkt = nullptr;
      if (geom->exportToWkt(&wkt) == OGRERR_NONE && wkt) {
        feat.geom_wkt = wkt;
      }
      CPLFree(wkt);
    }
    if (defn) {
      for (int i = 0; i < defn->GetFieldCount(); ++i) {
        OGRFieldDefn* fld = defn->GetFieldDefn(i);
        if (!fld || ogr->IsFieldNull(i)) {
          continue;
        }
        const char* name = fld->GetNameRef();
        if (name) {
          feat.attrs[name] = ogr->GetFieldAsString(i);
        }
      }
    }
    set.features.push_back(std::move(feat));
    OGRFeature::DestroyFeature(ogr);
  }
  set.count = static_cast<int>(set.features.size());
  set.move_first();
  return set;
}

bool SdbdHandler::open_dataset(const std::string& body, int* status,
                               std::string* response) {
  if (body_has_forbidden_sql(body)) {
    write_error(400, "forbidden_sql", "sql keys are not allowed", status,
                response);
    return false;
  }
  Json json;
  if (!body.empty() &&
      (!parse_json(body, &json, nullptr) || json.type != Json::kObject)) {
    write_error(400, "bad_request", "invalid JSON", status, response);
    return false;
  }
  const std::string name = make_sdbd_open_name(json);
  close_dataset();
  dataset_ = static_cast<GDALDataset*>(
      GDALOpenEx(name.c_str(), GDAL_OF_VECTOR | GDAL_OF_RASTER | GDAL_OF_UPDATE,
                 nullptr, nullptr, nullptr));
  if (!dataset_) {
    GDALDriver* drv = GetGDALDriverManager()->GetDriverByName(kSdbdDriverName);
    if (drv) {
      dataset_ = drv->Create(name.c_str(), 0, 0, 0, GDT_Unknown, nullptr);
    }
  }
  if (!dataset_) {
    write_error(400, "bad_request",
                "SDBD open failed (missing inner driver or target)", status,
                response);
    return false;
  }
  write_ok("{\"ok\":true}", status, response);
  return true;
}

bool SdbdHandler::create_layer(const std::string& body, int* status,
                               std::string* response) {
  if (!dataset_) {
    write_error(400, "bad_request", "datasource not open", status, response);
    return false;
  }
  if (body_has_forbidden_sql(body)) {
    write_error(400, "forbidden_sql", "sql keys are not allowed", status,
                response);
    return false;
  }
  LayerInfo info;
  if (!parse_layer_info(body, &info, nullptr)) {
    write_error(400, "bad_request", "invalid LayerInfo", status, response);
    return false;
  }
  if (dataset_->GetLayerByName(info.name.c_str())) {
    write_error(400, "bad_request", "layer exists", status, response);
    return false;
  }
  OGRSpatialReference srs;
  OGRSpatialReference* srs_ptr = nullptr;
  if (!info.crs.empty() &&
      srs.SetFromUserInput(info.crs.c_str()) == OGRERR_NONE) {
    srs_ptr = &srs;
  }
  OGRLayer* lyr = dataset_->CreateLayer(info.name.c_str(), srs_ptr,
                                        geom_type_of(info.geom_type), nullptr);
  if (!lyr) {
    write_error(400, "bad_request", "CreateLayer failed", status, response);
    return false;
  }
  for (const FieldInfo& fld : info.fields) {
    OGRFieldDefn defn(fld.name.c_str(), field_type_of(fld.type));
    lyr->CreateField(&defn);
  }
  write_ok(layer_info_to_json(fill_layer_info(lyr)), status, response);
  return true;
}

bool SdbdHandler::append_features(const std::string& body, int* status,
                                  std::string* response) {
  if (!dataset_) {
    write_error(400, "bad_request", "datasource not open", status, response);
    return false;
  }
  if (body_has_forbidden_sql(body)) {
    write_error(400, "forbidden_sql", "sql keys are not allowed", status,
                response);
    return false;
  }
  FeatureSet set;
  if (!parse_feature_set(body, &set, nullptr) || set.layer.empty()) {
    write_error(400, "bad_request", "invalid FeatureSet", status, response);
    return false;
  }
  OGRLayer* lyr = dataset_->GetLayerByName(set.layer.c_str());
  if (!lyr) {
    write_error(404, "layer_not_found", "unknown layer", status, response);
    return false;
  }
  int added = 0;
  for (const Feature& feat : set.features) {
    OGRFeature ogr(lyr->GetLayerDefn());
    if (!feat.geom_wkt.empty()) {
      OGRGeometry* geom = nullptr;
      if (OGRGeometryFactory::createFromWkt(feat.geom_wkt.c_str(), nullptr,
                                            &geom) != OGRERR_NONE) {
        OGRGeometryFactory::destroyGeometry(geom);
        write_error(400, "bad_request", "invalid geom_wkt", status, response);
        return false;
      }
      ogr.SetGeometryDirectly(geom);
    }
    for (const auto& kv : feat.attrs) {
      ogr.SetField(kv.first.c_str(), kv.second.c_str());
    }
    if (lyr->CreateFeature(&ogr) == OGRERR_NONE) {
      ++added;
    }
  }
  std::ostringstream o;
  o << "{\"ok\":true,\"count\":" << added << '}';
  write_ok(o.str(), status, response);
  return true;
}

void SdbdHandler::handle(const std::string& method, const std::string& raw,
                         const std::string& body, int* status,
                         std::string* response) {
  const std::string path = path_only(raw);
  if (!ready_) {
    write_error(503, "not_ready", "sdbd engine not loaded", status, response);
    return;
  }

  if (method == "GET" && path == "/sdbd/api/v1/health") {
    write_ok("{\"ok\":true,\"service\":\"sdbd\"}", status, response);
    return;
  }

  if (method == "POST" && path == "/sdbd/api/v1/datasource/open") {
    open_dataset(body, status, response);
    return;
  }
  if (method == "POST" && path == "/sdbd/api/v1/datasource/close") {
    close_dataset();
    write_ok("{\"ok\":true}", status, response);
    return;
  }

  if (method == "GET" && path == "/sdbd/api/v1/catalog") {
    const auto layers = catalog();
    std::ostringstream o;
    o << '[';
    for (std::size_t i = 0; i < layers.size(); ++i) {
      if (i) {
        o << ',';
      }
      o << layer_info_to_json(layers[i]);
    }
    o << ']';
    write_ok(o.str(), status, response);
    return;
  }

  const std::string layer_prefix = "/sdbd/api/v1/layers/";
  if (method == "GET" && path.rfind(layer_prefix, 0) == 0) {
    const std::string name = path.substr(layer_prefix.size());
    LayerInfo info = layer_info_of(name);
    if (info.name.empty()) {
      write_error(404, "layer_not_found", "unknown layer", status, response);
      return;
    }
    write_ok(layer_info_to_json(info), status, response);
    return;
  }

  if (method != "POST") {
    write_error(400, "bad_request", "unsupported method", status, response);
    return;
  }

  if (path == "/sdbd/api/v1/layers") {
    create_layer(body, status, response);
    return;
  }
  if (path == "/sdbd/api/v1/recordset/append") {
    append_features(body, status, response);
    return;
  }

  if (path == "/sdbd/api/v1/recordset/open" ||
      path == "/sdbd/api/v1/recordset/query") {
    OpenRequest req;
    if (!parse_open_request(body, &req, status, response)) {
      return;
    }
    if (!dataset_ || !dataset_->GetLayerByName(req.layer.c_str())) {
      write_error(404, "layer_not_found", "unknown layer", status, response);
      return;
    }
    const LayerInfo info = layer_info_of(req.layer);
    if (!req.crs.empty() && !info.crs.empty() && req.crs != info.crs) {
      write_error(400, "bad_request", "crs mismatch (no reproject)", status,
                  response);
      return;
    }
    const auto hits = query_hits(req);
    if (path == "/sdbd/api/v1/recordset/query") {
      write_ok(feature_set_to_json(features_of(req.layer, info.crs, 0, hits)),
               status, response);
      return;
    }
    const std::string handle = std::to_string(next_handle_++);
    cursors_[handle] = Cursor{req.layer, info.crs, hits};
    std::ostringstream o;
    o << "{\"ok\":true,\"handle\":\"" << handle
      << "\",\"schema\":" << layer_info_to_json(info)
      << ",\"matched\":" << hits.size() << '}';
    write_ok(o.str(), status, response);
    return;
  }

  if (path == "/sdbd/api/v1/recordset/fetch") {
    if (body_has_forbidden_sql(body)) {
      write_error(400, "forbidden_sql", "sql keys are not allowed", status,
                  response);
      return;
    }
    Json json;
    if (!parse_json(body, &json, nullptr) || json.type != Json::kObject) {
      write_error(400, "bad_request", "invalid JSON", status, response);
      return;
    }
    const std::string handle = json.string("handle");
    auto it = cursors_.find(handle);
    if (it == cursors_.end()) {
      write_error(404, "handle_not_found", "unknown handle", status, response);
      return;
    }
    int offset = static_cast<int>(json.number("offset"));
    int limit = json.has("limit") ? static_cast<int>(json.number("limit"))
                                  : kSdbdFetchDefault;
    if (limit > kSdbdFetchMax || limit > kSdbdLimitHardCap) {
      write_error(400, "limit_exceeded", "fetch limit exceeds cap", status,
                  response);
      return;
    }
    if (offset < 0) {
      offset = 0;
    }
    std::vector<std::int64_t> slice;
    for (int i = offset;
         i < static_cast<int>(it->second.fids.size()) && i < offset + limit;
         ++i) {
      slice.push_back(it->second.fids[static_cast<std::size_t>(i)]);
    }
    write_ok(feature_set_to_json(
                 features_of(it->second.layer, it->second.crs, offset, slice)),
             status, response);
    return;
  }

  if (path == "/sdbd/api/v1/recordset/close") {
    Json json;
    if (!parse_json(body, &json, nullptr) || json.type != Json::kObject) {
      write_error(400, "bad_request", "invalid JSON", status, response);
      return;
    }
    const std::string handle = json.string("handle");
    if (cursors_.erase(handle) == 0) {
      write_error(404, "handle_not_found", "unknown handle", status, response);
      return;
    }
    write_ok("{\"ok\":true}", status, response);
    return;
  }

  write_error(404, "layer_not_found", "unknown route", status, response);
}

}  // namespace datasource
}  // namespace gis
