// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_CAMERA_VIEW_NAVIGATION_H_
#define CONTENT_BROWSER_CAMERA_VIEW_NAVIGATION_H_

#include <cstddef>
#include <string>
#include <vector>

#include "content/public/map_layer_types.h"

namespace content {

// One session bookmark. Not written to the project file.
struct ViewBookmark {
  std::string label;
  content::Extent2 extent{};
};

// Shared 2D extent history, zoom targets, session bookmarks, and status-bar
// scale. Previous/next move an index. A failed target does not push.
class ViewNavigation {
 public:
  static constexpr size_t kExtentCap = 32;

  void reset(const content::Extent2& extent);

  const content::Extent2& extent() const { return extent_; }
  const std::string& status() const { return status_; }
  const std::vector<ViewBookmark>& bookmarks() const { return bookmarks_; }
  size_t history_size() const { return history_.size(); }
  size_t history_index() const { return index_; }

  // Push the current extent and adopt |next| when it differs.
  bool commit(const content::Extent2& next);

  bool previous();
  bool next();

  // |target| null or empty: status |empty_status|, extent unchanged.
  bool zoom_to(const content::Extent2* target, const char* empty_status);
  // Failure strings live here: "No active layer" / "No selection".
  bool zoom_layer(const content::Extent2* target);
  bool zoom_selection(const content::Extent2* target);
  void note_no_feature();

  // Empty |label| becomes "Bookmark N". Collisions gain " (2)", " (3)", …
  void add_bookmark(std::string label = {});
  bool go_bookmark(size_t index);

  std::string scale_text(int viewport_width_px) const;

 private:
  std::string unique_label(const std::string& base) const;

  std::vector<content::Extent2> history_;
  size_t index_ = 0;
  content::Extent2 extent_{};
  std::vector<ViewBookmark> bookmarks_;
  int bookmark_serial_ = 0;
  std::string status_;
};

bool extents_equal(const content::Extent2& a, const content::Extent2& b);

// Status-bar scale "1:N" for |extent| at |viewport_width_px|. Width 0 is "1:—".
std::string format_view_scale(const content::Extent2& extent,
                              int viewport_width_px);

}  // namespace content

#endif  // CONTENT_BROWSER_CAMERA_VIEW_NAVIGATION_H_
