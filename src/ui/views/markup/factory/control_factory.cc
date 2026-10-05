// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/markup/factory/control_factory.h"

#include <cctype>
#include <utility>

#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {
namespace {

std::string to_lower(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    out.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  }
  return out;
}

}  // namespace

MarkupAttrs::MarkupAttrs() = default;
MarkupAttrs::MarkupAttrs(const MarkupAttrs&) = default;
MarkupAttrs::MarkupAttrs(MarkupAttrs&&) noexcept = default;
MarkupAttrs& MarkupAttrs::operator=(const MarkupAttrs&) = default;
MarkupAttrs& MarkupAttrs::operator=(MarkupAttrs&&) noexcept = default;
MarkupAttrs::~MarkupAttrs() = default;

std::string MarkupAttrs::get(std::string_view key,
                             std::string_view fallback) const {
  const auto it = values.find(std::string(key));
  if (it == values.end()) {
    return std::string(fallback);
  }
  return it->second;
}

ControlFactory::ControlFactory() = default;

void ControlFactory::register_tag(std::string_view tag, Creator creator) {
  creators_[to_lower(tag)] = std::move(creator);
}

bool ControlFactory::has_tag(std::string_view tag) const {
  return creators_.find(to_lower(tag)) != creators_.end();
}

std::unique_ptr<View> ControlFactory::create(std::string_view tag,
                                             const MarkupAttrs& attrs) const {
  const auto it = creators_.find(to_lower(tag));
  if (it == creators_.end()) {
    return nullptr;
  }
  return it->second(tag, attrs);
}

std::vector<std::string> ControlFactory::registered_tags() const {
  std::vector<std::string> out;
  out.reserve(creators_.size());
  for (const auto& [tag, _] : creators_) {
    out.push_back(tag);
  }
  return out;
}

}  // namespace views
}  // namespace ui
