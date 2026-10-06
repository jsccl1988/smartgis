// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_RUNTIME_PLUGIN_PLAYBACK_H_
#define APP_VIEWS_RUNTIME_PLUGIN_PLAYBACK_H_

#include <functional>
#include <string>
#include <string_view>

namespace app {

// Horizon ResultPlayback session: play/pause/fps plus which plugin last filled
// PluginHost::Playback. Payload buffers live in the product plugin; this type
// must not name flood / traffic / orthogrid stores.
class PluginPlayback {
 public:
  // rebuild=true: replace Host frames to |count|; always set_index(|index|).
  using HostPlaybackTick =
      std::function<void(int index, int count, bool rebuild)>;
  void bind_host_playback(HostPlaybackTick tick);

  void reset();

  const std::string& plugin_id() const { return plugin_id_; }
  const std::string& present_frame_id() const { return present_frame_id_; }
  // Short status label (id after "smartgis.", or "session").
  std::string label() const;

  int frame_count() const { return frame_count_; }
  int frame_index() const { return frame_index_; }
  double fps() const { return fps_; }
  bool looping() const { return looping_; }
  bool playing() const { return playing_; }

  void set_fps(double fps) { fps_ = fps > 0.0 ? fps : 12.0; }
  void set_looping(bool on) { looping_ = on; }
  void set_playing(bool on) { playing_ = on; }

  // Clamps to [0, frame_count-1] when frame_count > 0. Returns false if empty.
  bool set_frame_index(int index);

  // Product filled PluginHost::Playback. ResultPlayback ticks via
  // present_frame_id (see present_plugin_frame).
  void adopt_host_frames(std::string plugin_id,
                         std::string present_frame_id,
                         int frames);

  std::string playback_json() const;

 private:
  void notify_host_playback(bool rebuild_frames);

  HostPlaybackTick host_tick_;
  std::string plugin_id_;
  std::string present_frame_id_;
  int frame_count_ = 0;
  int frame_index_ = 0;
  double fps_ = 12.0;
  bool looping_ = true;
  bool playing_ = false;
};

}  // namespace app

#endif  // APP_VIEWS_RUNTIME_PLUGIN_PLAYBACK_H_
