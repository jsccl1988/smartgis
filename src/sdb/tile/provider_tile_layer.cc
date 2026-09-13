// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/tile/provider_tile_layer.h"

#include "base/core/core.h"

#include <cstring>

namespace sdb {
namespace tile {

ProviderTileLayer::ProviderTileLayer(std::shared_ptr<TileProvider> provider)
    : provider_(std::move(provider)) {
  SetLayerName("tile");
  SetSRS("EPSG:3857");
}

ProviderTileLayer::~ProviderTileLayer() {
  clear_tiles();
}

void ProviderTileLayer::clear_tiles() {
  for (SmtTile* t : tiles_) {
    if (t) {
      SMT_SAFE_DELETE_A(t->pTileBuf);
      SMT_SAFE_DELETE(t);
    }
  }
  tiles_.clear();
  cursor_ = 0;
}

void ProviderTileLayer::adopt_images(std::vector<TileImage> images) {
  clear_tiles();
  tiles_.reserve(images.size());
  long next_id = 1;
  for (TileImage& img : images) {
    auto* tile = new SmtTile();
    tile->lID = next_id++;
    tile->rtTileRect = img.world_rect;
    tile->lImageCode = img.image_code;
    tile->bVisible = true;
    if (!img.bytes.empty()) {
      tile->lTileBufSize = static_cast<long>(img.bytes.size());
      tile->pTileBuf = new char[static_cast<size_t>(tile->lTileBufSize)];
      std::memcpy(tile->pTileBuf, img.bytes.data(),
                  static_cast<size_t>(tile->lTileBufSize));
    }
    tiles_.push_back(tile);
  }
  CalEnvelope();
  m_bOpen = true;
}

bool ProviderTileLayer::refresh_visible(const Viewport& viewport,
                                        int timeout_sec) {
  if (!provider_ || !provider_->is_open()) {
    return false;
  }
  adopt_images(provider_->fetch_visible(viewport, timeout_sec));
  return !tiles_.empty();
}

void ProviderTileLayer::set_images(std::vector<TileImage> images) {
  adopt_images(std::move(images));
}

bool ProviderTileLayer::Create() {
  m_bOpen = true;
  return true;
}

bool ProviderTileLayer::Open(const char* szLayerArchiveName) {
  if (!provider_) {
    return false;
  }
  if (szLayerArchiveName && szLayerArchiveName[0]) {
    if (!provider_->open_xyz(szLayerArchiveName)) {
      return false;
    }
  }
  m_bOpen = provider_->is_open();
  return m_bOpen;
}

bool ProviderTileLayer::Close() {
  clear_tiles();
  m_bOpen = false;
  return true;
}

bool ProviderTileLayer::Fetch(eSmtFetchType /*type*/) {
  return m_bOpen;
}

void ProviderTileLayer::CalEnvelope() {
  m_lyrEnv = Envelope();
  for (const SmtTile* t : tiles_) {
    if (!t) {
      continue;
    }
    m_lyrEnv.merge(t->rtTileRect.lb.x, t->rtTileRect.lb.y);
    m_lyrEnv.merge(t->rtTileRect.rt.x, t->rtTileRect.rt.y);
  }
}

int ProviderTileLayer::GetTileCount() const {
  return static_cast<int>(tiles_.size());
}

void ProviderTileLayer::MoveFirst() const {
  cursor_ = 0;
}

void ProviderTileLayer::MoveNext() const {
  ++cursor_;
}

void ProviderTileLayer::MoveLast() const {
  cursor_ = tiles_.empty() ? 0 : static_cast<int>(tiles_.size()) - 1;
}

void ProviderTileLayer::Delete() {
  if (cursor_ < 0 || cursor_ >= static_cast<int>(tiles_.size())) {
    return;
  }
  SmtTile* t = tiles_[static_cast<size_t>(cursor_)];
  if (t) {
    SMT_SAFE_DELETE_A(t->pTileBuf);
    SMT_SAFE_DELETE(t);
  }
  tiles_.erase(tiles_.begin() + cursor_);
  if (cursor_ >= static_cast<int>(tiles_.size()) && cursor_ > 0) {
    --cursor_;
  }
  CalEnvelope();
}

bool ProviderTileLayer::IsEnd() const {
  return cursor_ >= static_cast<int>(tiles_.size());
}

void ProviderTileLayer::DeleteAll() {
  clear_tiles();
  CalEnvelope();
}

long ProviderTileLayer::AppendTile(const SmtTile* pTile, bool bClone) {
  if (!pTile) {
    return SMT_ERR_INVALID_PARAM;
  }
  auto* tile = new SmtTile();
  *tile = *pTile;
  if (bClone && pTile->pTileBuf && pTile->lTileBufSize > 0) {
    tile->pTileBuf = new char[static_cast<size_t>(pTile->lTileBufSize)];
    std::memcpy(tile->pTileBuf, pTile->pTileBuf,
                static_cast<size_t>(pTile->lTileBufSize));
  } else if (!bClone) {
    // Take ownership of caller's buffer pointer.
    tile->pTileBuf = pTile->pTileBuf;
  } else {
    tile->pTileBuf = nullptr;
    tile->lTileBufSize = 0;
  }
  tiles_.push_back(tile);
  CalEnvelope();
  return SMT_ERR_NONE;
}

long ProviderTileLayer::UpdateTile(const SmtTile* pTile) {
  if (!pTile) {
    return SMT_ERR_INVALID_PARAM;
  }
  for (SmtTile* t : tiles_) {
    if (t && t->lID == pTile->lID) {
      SMT_SAFE_DELETE_A(t->pTileBuf);
      *t = *pTile;
      if (pTile->pTileBuf && pTile->lTileBufSize > 0) {
        t->pTileBuf = new char[static_cast<size_t>(pTile->lTileBufSize)];
        std::memcpy(t->pTileBuf, pTile->pTileBuf,
                    static_cast<size_t>(pTile->lTileBufSize));
      } else {
        t->pTileBuf = nullptr;
        t->lTileBufSize = 0;
      }
      CalEnvelope();
      return SMT_ERR_NONE;
    }
  }
  return SMT_ERR_FAILURE;
}

long ProviderTileLayer::DeleteTile(const SmtTile* pTile) {
  if (!pTile) {
    return SMT_ERR_INVALID_PARAM;
  }
  for (size_t i = 0; i < tiles_.size(); ++i) {
    if (tiles_[i] && tiles_[i]->lID == pTile->lID) {
      SMT_SAFE_DELETE_A(tiles_[i]->pTileBuf);
      SMT_SAFE_DELETE(tiles_[i]);
      tiles_.erase(tiles_.begin() + static_cast<std::ptrdiff_t>(i));
      CalEnvelope();
      return SMT_ERR_NONE;
    }
  }
  return SMT_ERR_FAILURE;
}

SmtTile* ProviderTileLayer::GetTile() const {
  if (cursor_ < 0 || cursor_ >= static_cast<int>(tiles_.size())) {
    return nullptr;
  }
  return tiles_[static_cast<size_t>(cursor_)];
}

SmtTile* ProviderTileLayer::GetTile(int index) const {
  if (index < 0 || index >= static_cast<int>(tiles_.size())) {
    return nullptr;
  }
  return tiles_[static_cast<size_t>(index)];
}

SmtTile* ProviderTileLayer::GetTileByID(uint unID) const {
  for (SmtTile* t : tiles_) {
    if (t && static_cast<uint>(t->lID) == unID) {
      return t;
    }
  }
  return nullptr;
}

}  // namespace tile
}  // namespace sdb
