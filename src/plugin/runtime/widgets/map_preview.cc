// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/widgets/map_preview.h"

#include <memory>
#include <string>

#include "ui/views/kernel/layout/layout.h"

namespace plugin {
namespace {

std::string from_wide(const wchar_t* w) {
  if (!w || !*w) {
    return {};
  }
  const int n =
      WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
  if (n <= 1) {
    return {};
  }
  std::string s(static_cast<size_t>(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
  s.resize(static_cast<size_t>(n - 1));
  return s;
}

}  // namespace

MapPreviewView::MapPreviewView() {
  auto box = std::make_unique<ui::views::BoxLayout>(
      ui::views::BoxLayout::Orientation::kVertical);
  auto vp = std::make_unique<ui::views::DrawHost>();
  draw_host_ = vp.get();
  auto bar = std::make_unique<ui::views::StatusBar>();
  status_ = bar.get();
  box->set_flex_for_view(draw_host_, 1);
  set_layout_manager(std::move(box));
  add_child(std::move(vp));
  add_child(std::move(bar));
  set_preferred_size({480, 320});
}

bool MapPreviewView::export_bmp(const std::string& path) const {
  return draw_host_ && draw_host_->export_bmp(path);
}

bool MapPreviewView::open_document(std::string_view path) {
  path_.assign(path);
  if (status_) {
    const wchar_t* hang = draw_host_ ? draw_host_->status_text() : nullptr;
    if (hang && hang[0]) {
      status_->set_status(from_wide(hang));
    } else {
      status_->set_status(path_);
    }
  }
  return true;
}

ui::views::DrawHost* MapPreviewView::draw_host() {
  return draw_host_;
}

ui::views::StatusBar* MapPreviewView::status_bar() {
  return status_;
}

const std::string& MapPreviewView::document_path() const {
  return path_;
}

}  // namespace plugin
