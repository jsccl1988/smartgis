// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/browser/plugin/playback.h"

#include <algorithm>
#include <sstream>
#include <string_view>
#include <utility>

namespace app {

void PluginPlayback::bind_host_playback(HostPlaybackTick tick) {
  host_tick_ = std::move(tick);
  notify_host_playback(true);
}

void PluginPlayback::notify_host_playback(bool rebuild_frames) {
  if (host_tick_) {
    host_tick_(frame_index_, frame_count_, rebuild_frames);
  }
}

void PluginPlayback::reset() {
  plugin_id_.clear();
  present_frame_id_.clear();
  frame_count_ = 0;
  frame_index_ = 0;
  fps_ = 12.0;
  looping_ = true;
  playing_ = false;
  notify_host_playback(true);
}

std::string PluginPlayback::label() const {
  constexpr std::string_view kPrefix = "smartgis.";
  if (plugin_id_.starts_with(kPrefix)) {
    return plugin_id_.substr(kPrefix.size());
  }
  if (!plugin_id_.empty()) {
    return plugin_id_;
  }
  return "session";
}

bool PluginPlayback::set_frame_index(int index) {
  if (frame_count_ <= 0) {
    return false;
  }
  frame_index_ = std::clamp(index, 0, frame_count_ - 1);
  notify_host_playback(false);
  return true;
}

void PluginPlayback::adopt_host_frames(std::string plugin_id,
                                       std::string present_frame_id,
                                       int frames) {
  if (plugin_id.empty() || frames <= 0) {
    return;
  }
  plugin_id_ = std::move(plugin_id);
  present_frame_id_ = std::move(present_frame_id);
  frame_count_ = frames;
  frame_index_ = frames - 1;
  playing_ = false;
  notify_host_playback(false);
}

std::string PluginPlayback::playback_json() const {
  std::ostringstream o;
  o << "{\"plugin\":\"" << plugin_id_ << "\",\"present_frame\":\""
    << present_frame_id_ << "\",\"frame_count\":" << frame_count_
    << ",\"frame_index\":" << frame_index_ << ",\"fps\":" << fps_
    << ",\"looping\":" << (looping_ ? "true" : "false") << "}";
  return o.str();
}

}  // namespace app
