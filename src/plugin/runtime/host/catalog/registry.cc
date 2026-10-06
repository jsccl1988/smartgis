// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/host/catalog/registry.h"

#include "content/public/plugin_host.h"

#include <algorithm>
#include <fstream>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace plugin {
namespace {

// Builtin start() lives in the EXE; Registry is in plugin_host.dll. A stale
// IAT / vtable slot shows up as call-to-0 inside the registrar — do not take
// the process down (harness ensure_builtins).
int invoke_builtin_start(bool (*start)(content::PluginHost*),
                         content::PluginHost* host) {
  if (!start) {
    return -1;
  }
  __try {
    return start(host) ? 1 : 0;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return -1;
  }
}

}  // namespace

Registry::Registry() = default;

Registry* registry_new() {
  return new Registry();
}

void registry_delete(Registry* r) {
  delete r;
}

const std::string& Registry::last_error() const {
  return last_error_;
}

PluginRecord* Registry::find_mut(std::string_view id) {
  auto it = records_.find(std::string(id));
  return it == records_.end() ? nullptr : &it->second;
}

const PluginRecord* Registry::find(std::string_view id) const {
  auto it = records_.find(std::string(id));
  return it == records_.end() ? nullptr : &it->second;
}

std::vector<PluginRecord> Registry::list() const {
  std::vector<PluginRecord> out;
  out.reserve(records_.size());
  for (const auto& kv : records_) {
    out.push_back(kv.second);
  }
  return out;
}

bool Registry::add_builtin_manifest(const char* id, const char* name) {
  last_error_.clear();
  if (!id || !id[0]) {
    last_error_ = "empty id";
    return false;
  }
  Manifest m;
  m.id = id;
  m.name = (name && name[0]) ? name : id;
  m.version = "1.0.0";
  m.api_version = 2;
  m.kind = PluginKind::kBuiltin;
  return add_manifest(m, TrustClass::kBuiltin);
}

bool Registry::add_manifest(const Manifest& m, TrustClass trust) {
  last_error_.clear();
  if (m.id.empty()) {
    last_error_ = "empty id";
    return false;
  }
  if (records_.contains(m.id)) {
    last_error_ = "duplicate id";
    return false;
  }
  PluginRecord rec;
  rec.manifest = m;
  rec.trust = trust;
  rec.state = PluginState::kDisabled;
  for (const std::string& id : trusted_unsigned_ids_) {
    if (id == m.id && rec.trust == TrustClass::kDenied) {
      rec.trust = TrustClass::kUnsignedTrusted;
    }
  }
  for (const std::string& id : enabled_ids_) {
    if (id == m.id && rec.trust != TrustClass::kDenied) {
      rec.state = PluginState::kEnabled;
    }
  }
  records_.emplace(m.id, std::move(rec));
  return true;
}

bool Registry::trust_unsigned(std::string_view id) {
  last_error_.clear();
  if (id.empty()) {
    last_error_ = "empty id";
    return false;
  }
  PluginRecord* rec = find_mut(id);
  if (!rec) {
    last_error_ = "unknown plugin";
    return false;
  }
  if (rec->manifest.kind == PluginKind::kBuiltin) {
    last_error_ = "builtin already allowed";
    return false;
  }
  rec->trust = TrustClass::kUnsignedTrusted;
  if (std::find(trusted_unsigned_ids_.begin(), trusted_unsigned_ids_.end(),
                rec->manifest.id) == trusted_unsigned_ids_.end()) {
    trusted_unsigned_ids_.push_back(rec->manifest.id);
  }
  save_state();
  return true;
}

bool Registry::register_builtin_hooks(std::string_view id,
                                      bool (*start)(content::PluginHost*),
                                      void (*stop)()) {
  last_error_.clear();
  if (id.empty() || !start) {
    last_error_ = "bad hooks";
    return false;
  }
  Hooks h;
  h.start = start;
  h.stop = stop;
  hooks_[std::string(id)] = h;
  return true;
}

void Registry::set_python_starter(
    std::function<bool(const PluginRecord&, content::PluginHost*)> start,
    std::function<void(const PluginRecord&)> stop) {
  python_start_ = std::move(start);
  python_stop_ = std::move(stop);
}

void Registry::set_native_starter(
    std::function<bool(const PluginRecord&, content::PluginHost*)> start,
    std::function<void(const PluginRecord&)> stop,
    std::function<int(const PluginRecord&, std::string_view, std::string_view)>
        run) {
  native_start_ = std::move(start);
  native_stop_ = std::move(stop);
  native_run_ = std::move(run);
}

bool Registry::set_directory(std::string_view id, std::string directory) {
  PluginRecord* rec = find_mut(id);
  if (!rec) {
    last_error_ = "unknown plugin";
    return false;
  }
  rec->directory = std::move(directory);
  return true;
}

bool Registry::start_plugin(PluginRecord* rec, content::PluginHost* host) {
  if (!rec) {
    return false;
  }
  auto hit = hooks_.find(rec->manifest.id);
  if (hit != hooks_.end() && hit->second.start) {
    const int rc = invoke_builtin_start(hit->second.start, host);
    if (rc < 0) {
      rec->state = PluginState::kError;
      last_error_ = "start crashed";
      return false;
    }
    if (rc == 0) {
      rec->state = PluginState::kError;
      last_error_ = "start failed";
      return false;
    }
    rec->state = PluginState::kEnabled;
    return true;
  }

  switch (rec->manifest.kind) {
    case PluginKind::kBuiltin:
      rec->state = PluginState::kEnabled;
      return true;
    case PluginKind::kLegacyAm:
      rec->state = PluginState::kError;
      last_error_ = "legacy am is not supported";
      return false;
    case PluginKind::kPython:
      if (python_start_) {
        if (!python_start_(*rec, host)) {
          rec->state = PluginState::kError;
          last_error_ = "python start failed";
          return false;
        }
        rec->state = PluginState::kEnabled;
        return true;
      }
      last_error_ = "python not wired";
      rec->state = PluginState::kError;
      return false;
    case PluginKind::kNative:
      if (native_start_) {
        if (!native_start_(*rec, host)) {
          rec->state = PluginState::kLoadFailed;
          last_error_ = "native init failed";
          return false;
        }
        rec->state = PluginState::kEnabled;
        return true;
      }
      last_error_ = "native not wired";
      rec->state = PluginState::kError;
      return false;
  }
  last_error_ = "unknown kind";
  return false;
}

void Registry::stop_plugin(PluginRecord* rec, content::PluginHost* host) {
  if (!rec) {
    return;
  }
  auto hit = hooks_.find(rec->manifest.id);
  if (hit != hooks_.end() && hit->second.stop) {
    hit->second.stop();
  } else if (rec->manifest.kind == PluginKind::kPython && python_stop_) {
    python_stop_(*rec);
  } else if (rec->manifest.kind == PluginKind::kNative && native_stop_) {
    native_stop_(*rec);
  }
  (void)host;
}

bool Registry::set_enabled(std::string_view id, bool enabled,
                           content::PluginHost* host) {
  last_error_.clear();
  PluginRecord* rec = find_mut(id);
  if (!rec) {
    last_error_ = "unknown plugin";
    return false;
  }
  if (enabled) {
    if (rec->trust == TrustClass::kDenied) {
      last_error_ = "unsigned plugin is not trusted";
      return false;
    }
    if (rec->state == PluginState::kEnabled) {
      return true;
    }
    if (!host) {
      rec->state = PluginState::kEnabled;
      if (std::find(enabled_ids_.begin(), enabled_ids_.end(), rec->manifest.id) ==
          enabled_ids_.end()) {
        enabled_ids_.push_back(rec->manifest.id);
      }
      save_state();
      return true;
    }
    const bool started = start_plugin(rec, host);
    if (started) {
      if (std::find(enabled_ids_.begin(), enabled_ids_.end(), rec->manifest.id) ==
          enabled_ids_.end()) {
        enabled_ids_.push_back(rec->manifest.id);
      }
      save_state();
    }
    return started;
  }

  rec->state = PluginState::kDisabled;
  enabled_ids_.erase(std::remove(enabled_ids_.begin(), enabled_ids_.end(),
                                 rec->manifest.id),
                     enabled_ids_.end());
  if (host) {
    host->withdraw(id);
    stop_plugin(rec, host);
  }
  save_state();
  return true;
}

bool Registry::unload(std::string_view id, content::PluginHost* host) {
  last_error_.clear();
  PluginRecord* rec = find_mut(id);
  if (!rec) {
    last_error_ = "unknown plugin";
    return false;
  }
  if (rec->state == PluginState::kEnabled) {
    set_enabled(id, false, host);
  }
  records_.erase(std::string(id));
  return true;
}

namespace {

void write_string_array(rapidjson::Writer<rapidjson::StringBuffer>& w,
                        const char* key,
                        const std::vector<std::string>& ids) {
  w.Key(key);
  w.StartArray();
  for (const std::string& id : ids) {
    w.String(id.c_str(), static_cast<rapidjson::SizeType>(id.size()));
  }
  w.EndArray();
}

std::vector<std::string> parse_string_array(const rapidjson::Value& root,
                                            const char* key) {
  std::vector<std::string> out;
  if (!root.IsObject()) {
    return out;
  }
  const auto it = root.FindMember(key);
  if (it == root.MemberEnd() || !it->value.IsArray()) {
    return out;
  }
  for (const auto& v : it->value.GetArray()) {
    if (v.IsString()) {
      out.emplace_back(v.GetString(), v.GetStringLength());
    }
  }
  return out;
}

}  // namespace

void Registry::set_state_path(std::string path) {
  state_path_ = std::move(path);
}

bool Registry::save_state() const {
  if (state_path_.empty()) {
    return true;
  }
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  write_string_array(w, "trusted_unsigned", trusted_unsigned_ids_);
  write_string_array(w, "enabled", enabled_ids_);
  w.EndObject();
  std::ofstream out(state_path_, std::ios::binary);
  if (!out) {
    return false;
  }
  out.write(buf.GetString(), static_cast<std::streamsize>(buf.GetSize()));
  return static_cast<bool>(out);
}

bool Registry::load_state() {
  if (state_path_.empty()) {
    return false;
  }
  std::ifstream in(state_path_, std::ios::binary);
  if (!in) {
    return false;
  }
  const std::string body((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
  rapidjson::Document root;
  root.Parse(body.c_str());
  if (root.HasParseError() || !root.IsObject()) {
    return false;
  }
  trusted_unsigned_ids_ = parse_string_array(root, "trusted_unsigned");
  enabled_ids_ = parse_string_array(root, "enabled");
  for (const std::string& id : trusted_unsigned_ids_) {
    if (PluginRecord* rec = find_mut(id)) {
      if (rec->manifest.kind != PluginKind::kBuiltin) {
        rec->trust = TrustClass::kUnsignedTrusted;
      }
    }
  }
  for (const std::string& id : enabled_ids_) {
    if (PluginRecord* rec = find_mut(id)) {
      if (rec->trust != TrustClass::kDenied) {
        rec->state = PluginState::kEnabled;
      }
    }
  }
  return true;
}

}  // namespace plugin
