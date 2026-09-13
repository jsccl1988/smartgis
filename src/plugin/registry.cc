// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/registry.h"

#include "plugin/legacy_am.h"

#include "content/public/plugin_host.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace plugin {

Registry::Registry() = default;

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

bool Registry::start_plugin(PluginRecord* rec, content::PluginHost* host) {
  if (!rec) {
    return false;
  }
  auto hit = hooks_.find(rec->manifest.id);
  if (hit != hooks_.end() && hit->second.start) {
    if (!hit->second.start(host)) {
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
      if (!legacy_am_start(rec->manifest.id)) {
        rec->state = PluginState::kError;
        last_error_ = "legacy start failed";
        return false;
      }
      rec->state = PluginState::kEnabled;
      return true;
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
  } else if (rec->manifest.kind == PluginKind::kLegacyAm) {
    legacy_am_stop(rec->manifest.id);
  } else if (rec->manifest.kind == PluginKind::kPython && python_stop_) {
    python_stop_(*rec);
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
  if (rec->manifest.kind == PluginKind::kLegacyAm) {
    legacy_am_unload(id);
  }
  records_.erase(std::string(id));
  return true;
}

namespace {

std::string json_escape(const std::string& s) {
  std::string o;
  o.reserve(s.size());
  for (char c : s) {
    if (c == '"' || c == '\\') {
      o.push_back('\\');
    }
    o.push_back(c);
  }
  return o;
}

void write_string_array(std::ostringstream& os, const char* key,
                        const std::vector<std::string>& ids) {
  os << "\"" << key << "\":[";
  for (size_t i = 0; i < ids.size(); ++i) {
    if (i) {
      os << ",";
    }
    os << "\"" << json_escape(ids[i]) << "\"";
  }
  os << "]";
}

std::vector<std::string> parse_string_array(const std::string& body,
                                            const char* key) {
  std::vector<std::string> out;
  const std::string needle = std::string("\"") + key + "\"";
  const size_t k = body.find(needle);
  if (k == std::string::npos) {
    return out;
  }
  const size_t lb = body.find('[', k);
  const size_t rb = body.find(']', lb == std::string::npos ? k : lb);
  if (lb == std::string::npos || rb == std::string::npos) {
    return out;
  }
  size_t i = lb + 1;
  while (i < rb) {
    const size_t q1 = body.find('"', i);
    if (q1 == std::string::npos || q1 >= rb) {
      break;
    }
    const size_t q2 = body.find('"', q1 + 1);
    if (q2 == std::string::npos || q2 > rb) {
      break;
    }
    out.emplace_back(body.substr(q1 + 1, q2 - q1 - 1));
    i = q2 + 1;
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
  std::ostringstream os;
  os << "{";
  write_string_array(os, "trusted_unsigned", trusted_unsigned_ids_);
  os << ",";
  write_string_array(os, "enabled", enabled_ids_);
  os << "}";
  std::ofstream out(state_path_, std::ios::binary);
  if (!out) {
    return false;
  }
  out << os.str();
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
  trusted_unsigned_ids_ = parse_string_array(body, "trusted_unsigned");
  enabled_ids_ = parse_string_array(body, "enabled");
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
