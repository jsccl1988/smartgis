// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "algorithm/stat/value_set.h"

namespace stat {

long ValueSet::bind(std::string name, std::span<const double> values) {
  if (name.empty()) {
    return SMT_ERR_INVALID_PARAM;
  }
  values_[std::move(name)].assign(values.begin(), values.end());
  return SMT_ERR_NONE;
}

bool ValueSet::has(std::string_view name) const {
  return values_.contains(name);
}

std::span<double> ValueSet::get(std::string_view name) {
  const auto it = values_.find(name);
  if (it == values_.end()) {
    return {};
  }
  return std::span<double>(it->second);
}

std::span<const double> ValueSet::get(std::string_view name) const {
  const auto it = values_.find(name);
  if (it == values_.end()) {
    return {};
  }
  return std::span<const double>(it->second);
}

std::vector<std::string> ValueSet::names() const {
  std::vector<std::string> out;
  out.reserve(values_.size());
  for (const auto& [name, _] : values_) {
    out.push_back(name);
  }
  return out;
}

void ValueSet::clear() {
  values_.clear();
}

}  // namespace stat
