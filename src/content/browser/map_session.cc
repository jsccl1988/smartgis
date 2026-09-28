// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/map_session.h"

#include "content/public/map_contents.h"
#include "content/public/map_contents_observer.h"
#include "content/public/view_host.h"

namespace content {

MapSession::MapSession() = default;

MapSession::~MapSession() {
  prepare_close();
  clear_map_contents_observer();
}

void MapSession::init_hosts() {
  edit_host_ = std::make_unique<ViewHost>();
  data_host_ = std::make_unique<ViewHost>();
  scene_host_ = std::make_unique<ViewHost>();
  map_contents_.reset(MapContents::Create());
  if (map_contents_ && !map_contents_->StartRenderProcess()) {
    map_contents_.reset();
  }
}

void MapSession::prepare_close() {
  if (prepare_close_done_) {
    return;
  }
  prepare_close_done_ = true;
  edit_gestures_.detach();
  data_gestures_.detach();
  scene_gestures_.detach();
  scene3d_.abandon_mesh();
  scene3d_stereo_.release();
}

void MapSession::set_map_contents_observer(MapContentsObserver* observer) {
  if (map_contents_) {
    map_contents_->SetObserver(observer);
  }
}

void MapSession::clear_map_contents_observer() {
  if (map_contents_) {
    map_contents_->SetObserver(nullptr);
  }
}

}  // namespace content
