// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/geochem/commands.h"

#include <cmath>
#include <cstdlib>
#include <limits>
#include <memory>
#include <string>
#include <string_view>

#include "content/public/plugin_host.h"
#include "gis/analysis/geochem/grade.h"
#include "gis/analysis/geochem/idw.h"
#include "gis/analysis/geochem/samples.h"
#include "gis/analysis/geochem/stats.h"
#include "plugin/product/geochem/views/analyze_dialog.h"
#include "plugin/product/geochem/present/present.h"
#include "plugin/runtime/host/processing/operation_result.h"
#include "plugin/runtime/widgets/about_dialog.h"
#include "plugin/runtime/widgets/owned_dialog.h"
#include "plugin/runtime/widgets/present_surface_picker.h"
#include "tool/command/command.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.geochem";

gis::detail::GeochemSampleSet g_last_samples;
gis::detail::GeochemIdwResult g_last_idw;
GeochemCommit g_last_commit;
std::string g_last_element;

// Showcase IL runs analyze then stats; stats must not wipe the IDW heat
// underlay when republishing graded samples.
void attach_cached_idw(GeochemCommit* commit) {
  if (!commit || commit->has_idw) {
    return;
  }
  if (!g_last_idw.ok || g_last_idw.values.empty()) {
    return;
  }
  commit->idw = g_last_idw;
  commit->has_idw = true;
}

bool parse_args(std::string_view json, rapidjson::Document* out) {
  if (!out) {
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
  return true;
}

bool json_get_double(const rapidjson::Value& obj, const char* key, double* out) {
  if (!out || !key || !obj.IsObject()) {
    return false;
  }
  const auto it = obj.FindMember(key);
  if (it == obj.MemberEnd() || !it->value.IsNumber()) {
    return false;
  }
  *out = it->value.GetDouble();
  return true;
}

bool json_get_int(const rapidjson::Value& obj, const char* key, int* out) {
  if (!out || !key || !obj.IsObject()) {
    return false;
  }
  const auto it = obj.FindMember(key);
  if (it == obj.MemberEnd() || !it->value.IsNumber()) {
    return false;
  }
  *out = it->value.GetInt();
  return true;
}

bool json_get_bool(const rapidjson::Value& obj, const char* key, bool* out) {
  if (!out || !key || !obj.IsObject()) {
    return false;
  }
  const auto it = obj.FindMember(key);
  if (it == obj.MemberEnd() || !it->value.IsBool()) {
    return false;
  }
  *out = it->value.GetBool();
  return true;
}

gis::detail::GeochemSampleSet load_samples(const rapidjson::Document& args,
                                           const std::string& element,
                                           std::string* err) {
  gis::detail::GeochemSampleSet set;
  bool use_active = false;
  json_get_bool(args, "use_active_layer", &use_active);
  if (use_active) {
    if (err) {
      *err = "no_layer_reader";
    }
    set.error = "no_layer_reader";
    return set;
  }

  std::string vector_path;
  json_get_string(args, "vector", &vector_path);
  if (!vector_path.empty()) {
    set = gis::detail::load_geochem_vector(vector_path, element);
    if (!set.ok && err) {
      *err = set.error;
    }
    return set;
  }

  std::string input;
  if (!json_get_string(args, "input", &input) || input.empty()) {
    json_get_string(args, "path", &input);
  }
  if (input.empty()) {
    if (err) {
      *err = "no_input";
    }
    set.error = "no_input";
    return set;
  }
  set = gis::detail::load_geochem_csv(input);
  if (!set.ok && err) {
    *err = set.error;
  }
  return set;
}

bool publish(content::PluginHost* host, const GeochemCommit& commit,
             const char* op) {
  g_last_commit = commit;
  if (!host) {
    return true;
  }
  content::GisDocument* gis = host->gis_document();
  std::string err;
  if (!gis || !present_geochem(gis, commit, &err)) {
    set_operation_result(
        std::string("{\"error\":\"") +
        (err.empty() ? "no_geochem_seam" : err) + "\",\"op\":\"" + op + "\"}");
    return false;
  }
  (void)host->present_dataset(kPluginId, "", 0);
  return true;
}

std::string stats_json(const GeochemCommit& commit) {
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  w.Key("ok");
  w.Bool(true);
  w.Key("op");
  w.String("geochem.stats");
  w.Key("element");
  w.String(commit.element.c_str());
  w.Key("samples");
  w.Int(static_cast<int>(commit.samples.samples.size()));
  if (commit.has_stats && commit.stats.ok) {
    w.Key("count");
    w.Int(commit.stats.count);
    w.Key("mean");
    w.Double(commit.stats.mean);
    w.Key("background");
    w.Double(commit.stats.background);
    w.Key("threshold");
    w.Double(commit.stats.threshold);
    w.Key("min");
    w.Double(commit.stats.min_v);
    w.Key("max");
    w.Double(commit.stats.max_v);
  }
  if (commit.correlation.ok) {
    w.Key("correlation_r");
    w.Double(commit.correlation.r);
    w.Key("correlate");
    w.String(commit.correlation.element_b.c_str());
  }
  if (commit.legend.ok) {
    w.Key("classes");
    w.Int(static_cast<int>(commit.legend.classes.size()));
  }
  w.EndObject();
  return buf.GetString();
}

bool geochem_load(content::PluginHost* host, std::string_view args_json) {
  if (host) {
    return publish(host, g_last_commit, "geochem.load");
  }
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"geochem.load\"}");
    return false;
  }
  std::string element = "Cu";
  json_get_string(args, "element", &element);
  std::string err;
  gis::detail::GeochemSampleSet set = load_samples(args, element, &err);
  if (!set.ok) {
    set_operation_result(std::string("{\"error\":\"") +
                         (err.empty() ? "load_failed" : err) +
                         "\",\"op\":\"geochem.load\"}");
    return false;
  }
  g_last_samples = set;
  g_last_element = element;

  GeochemCommit commit;
  commit.samples = set;
  commit.element = element;
  commit.legend =
      gis::detail::build_geochem_grade_legend(set, element, 5);
  attach_cached_idw(&commit);
  if (!publish(host, commit, "geochem.load")) {
    return false;
  }
  set_operation_result(
      std::string("{\"ok\":true,\"op\":\"geochem.load\",\"samples\":") +
      std::to_string(set.samples.size()) + ",\"elements\":" +
      std::to_string(set.element_names.size()) + "}");
  return true;
}

bool geochem_stats(content::PluginHost* host, std::string_view args_json) {
  if (host) {
    return publish(host, g_last_commit, "geochem.stats");
  }
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"geochem.stats\"}");
    return false;
  }
  std::string element = g_last_element.empty() ? "Cu" : g_last_element;
  json_get_string(args, "element", &element);
  int bins = 10;
  double k_sigma = 2.0;
  json_get_int(args, "bins", &bins);
  json_get_double(args, "k_sigma", &k_sigma);

  std::string err;
  gis::detail::GeochemSampleSet set = load_samples(args, element, &err);
  if (!set.ok) {
    if (g_last_samples.ok) {
      set = g_last_samples;
    } else {
      set_operation_result(std::string("{\"error\":\"") +
                           (err.empty() ? "load_failed" : err) +
                           "\",\"op\":\"geochem.stats\"}");
      return false;
    }
  }
  g_last_samples = set;
  g_last_element = element;

  GeochemCommit commit;
  commit.samples = set;
  commit.element = element;
  commit.stats =
      gis::detail::compute_geochem_stats(set, element, bins, k_sigma);
  commit.has_stats = commit.stats.ok;
  if (!commit.stats.ok) {
    set_operation_result(
        std::string("{\"error\":\"") +
        (commit.stats.error.empty() ? "stats_failed" : commit.stats.error) +
        "\",\"op\":\"geochem.stats\"}");
    return false;
  }
  std::string corr_b;
  if (json_get_string(args, "correlate", &corr_b) && !corr_b.empty()) {
    commit.correlation =
        gis::detail::compute_geochem_correlation(set, element, corr_b);
  }
  int classes = 5;
  json_get_int(args, "classes", &classes);
  commit.legend =
      gis::detail::build_geochem_grade_legend(set, element, classes);
  // Keep prior IDW heat raster when stats republishes sample grades.
  attach_cached_idw(&commit);

  std::string stats_out;
  if (json_get_string(args, "output", &stats_out) && !stats_out.empty()) {
    if (!gis::detail::run_geochem_stats_op(args_json)) {
      // Still publish viz even if optional file write fails on path shape.
    }
  }

  if (!publish(host, commit, "geochem.stats")) {
    return false;
  }
  set_operation_result(stats_json(commit));
  return true;
}

bool geochem_analyze(content::PluginHost* host, std::string_view args_json) {
  if (host) {
    return publish(host, g_last_commit, "geochem.analyze");
  }
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"geochem.analyze\"}");
    return false;
  }
  std::string element = "Cu";
  json_get_string(args, "element", &element);
  int bins = 10;
  int cells = 64;
  int classes = 5;
  double k_sigma = 2.0;
  double power = 2.0;
  json_get_int(args, "bins", &bins);
  json_get_int(args, "cells", &cells);
  json_get_int(args, "classes", &classes);
  json_get_double(args, "k_sigma", &k_sigma);
  json_get_double(args, "power", &power);
  double threshold = std::numeric_limits<double>::quiet_NaN();
  json_get_double(args, "threshold", &threshold);

  std::string err;
  gis::detail::GeochemSampleSet set = load_samples(args, element, &err);
  if (!set.ok) {
    set_operation_result(std::string("{\"error\":\"") +
                         (err.empty() ? "load_failed" : err) +
                         "\",\"op\":\"geochem.analyze\"}");
    return false;
  }
  g_last_samples = set;
  g_last_element = element;

  GeochemCommit commit;
  commit.samples = set;
  commit.element = element;
  commit.stats =
      gis::detail::compute_geochem_stats(set, element, bins, k_sigma);
  commit.has_stats = commit.stats.ok;
  std::string corr_b;
  if (json_get_string(args, "correlate", &corr_b) && !corr_b.empty()) {
    commit.correlation =
        gis::detail::compute_geochem_correlation(set, element, corr_b);
  }
  commit.legend =
      gis::detail::build_geochem_grade_legend(set, element, classes);
  commit.idw =
      gis::detail::run_geochem_idw(set, element, cells, power, threshold,
                                  k_sigma);
  commit.has_idw = commit.idw.ok;
  if (!commit.idw.ok) {
    set_operation_result(
        std::string("{\"error\":\"") +
        (commit.idw.error.empty() ? "idw_failed" : commit.idw.error) +
        "\",\"op\":\"geochem.analyze\"}");
    return false;
  }
  g_last_idw = commit.idw;

  std::string output;
  json_get_string(args, "output", &output);
  std::string mask_out;
  json_get_string(args, "mask_output", &mask_out);
  if (!output.empty()) {
    (void)gis::detail::write_geochem_idw_geotiff(output, commit.idw, mask_out);
  }

  if (!publish(host, commit, "geochem.analyze")) {
    return false;
  }

  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  w.Key("ok");
  w.Bool(true);
  w.Key("op");
  w.String("geochem.analyze");
  w.Key("element");
  w.String(element.c_str());
  w.Key("samples");
  w.Int(static_cast<int>(set.samples.size()));
  w.Key("width");
  w.Int(commit.idw.width);
  w.Key("height");
  w.Int(commit.idw.height);
  w.Key("threshold");
  w.Double(commit.idw.threshold);
  if (commit.has_stats) {
    w.Key("background");
    w.Double(commit.stats.background);
    w.Key("mean");
    w.Double(commit.stats.mean);
  }
  if (commit.correlation.ok) {
    w.Key("correlation_r");
    w.Double(commit.correlation.r);
  }
  w.EndObject();
  set_operation_result(buf.GetString());
  return true;
}

bool geochem_style_apply(content::PluginHost* host, std::string_view args_json) {
  if (host) {
    return publish(host, g_last_commit, "geochem.style_apply");
  }
  rapidjson::Document args;
  parse_args(args_json, &args);
  std::string element = g_last_element.empty() ? "Cu" : g_last_element;
  if (args.IsObject()) {
    json_get_string(args, "element", &element);
  }
  if (!g_last_samples.ok) {
    set_operation_result(
        "{\"error\":\"no_samples\",\"op\":\"geochem.style_apply\"}");
    return false;
  }
  GeochemCommit commit;
  commit.samples = g_last_samples;
  commit.element = element;
  int classes = 5;
  if (args.IsObject()) {
    json_get_int(args, "classes", &classes);
  }
  commit.legend =
      gis::detail::build_geochem_grade_legend(g_last_samples, element, classes);
  attach_cached_idw(&commit);
  if (!publish(host, commit, "geochem.style_apply")) {
    return false;
  }
  set_operation_result(
      "{\"ok\":true,\"op\":\"geochem.style_apply\",\"classes\":" +
      std::to_string(commit.legend.classes.size()) + "}");
  return true;
}

void show_dialog(const wchar_t* title, std::unique_ptr<ui::views::View> body) {
  if (!body) {
    return;
  }
  show_owned_dialog(title, body->preferred_size().width,
                    body->preferred_size().height, std::move(body));
}

}  // namespace

bool register_geochem(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  if (!host->contribute_command(
          kPluginId, "geochem.analyze", "地球化学分析", "tools",
          [host](const tool::CommandArgs&) {
            return host->open_dialog("geochem.analyze");
          })) {
    return false;
  }
  if (!host->contribute_command(
          kPluginId, "geochem.stats", "地球化学统计", "tools",
          [host](const tool::CommandArgs&) {
            return host->open_dialog("geochem.analyze");
          })) {
    return false;
  }
  if (!host->contribute_command(
          kPluginId, "geochem.style_apply", "应用分级符号", "tools",
          [host](const tool::CommandArgs&) {
            return host->run_processing("geochem.style_apply", "{}");
          })) {
    return false;
  }
  if (!host->contribute_command(
          kPluginId, "geochem.about", "关于", "tools",
          [host](const tool::CommandArgs&) {
            return host->open_dialog("geochem.about");
          })) {
    return false;
  }
  if (!host->contribute_dialog(
          kPluginId, {"geochem.analyze", "地球化学分析"},
          [host](content::PluginHost*) {
            show_dialog(L"地球化学分析",
                        wrap_with_present_surface(
                            host, std::make_unique<AnalyzeDialog>(host)));
          })) {
    return false;
  }
  if (!host->contribute_dialog(
          kPluginId, {"geochem.about", "关于"},
          [](content::PluginHost*) {
            show_dialog(L"关于",
                        std::make_unique<AboutDialog>(
                            "Geochem\nSample grades + IDW heat raster "
                            "(native.geochem_stats / native.geochem_idw)"));
          })) {
    return false;
  }
  return host->contribute_processing(
             kPluginId, {"geochem.load", "Load geochem CSV / vector / layer"},
             geochem_load) &&
         host->contribute_processing(
             kPluginId, {"geochem.stats", "Geochem sample stats"},
             geochem_stats) &&
         host->contribute_processing(
             kPluginId, {"geochem.analyze", "Geochem IDW + grade + stats"},
             geochem_analyze) &&
         host->contribute_processing(
             kPluginId, {"geochem.style_apply", "Apply graded sample style"},
             geochem_style_apply) &&
         host->contribute_export_frame(
             kPluginId, {"geochem_samples", 114.18, 30.45, 114.36, 30.60});
}

}  // namespace plugin
