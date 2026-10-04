// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_GIS_LAYER_TILE_WRAP_H_
#define SMT_LEGACY_GIS_LAYER_TILE_WRAP_H_

#include <cstring>
#include <vector>

#include "gis/carto/tile/provider_tile_layer.h"
#include "legacy/gis/layer/layer.h"

namespace gis {

// Leftover TileLayer ABI over product ProviderTileLayer.
class LeftoverProviderTileLayer : public TileLayer {
 public:
  LeftoverProviderTileLayer(tile::ProviderTileLayer* inner, bool owns)
      : inner_(inner), owns_(owns && inner != nullptr) {
    if (inner_) {
      SetLayerName(inner_->GetLayerName());
      SetSRS(inner_->GetSRS());
      inner_->get_envelope(m_lyrEnv);
      m_bOpen = inner_->IsOpen();
      rebuild_tiles();
    }
  }

  ~LeftoverProviderTileLayer() override {
    clear_cache();
    if (owns_) {
      delete inner_;
    }
    inner_ = nullptr;
  }

  tile::ProviderTileLayer* inner() { return inner_; }
  const tile::ProviderTileLayer* inner() const { return inner_; }
  tile::ProviderTileLayer* release_inner() {
    owns_ = false;
    tile::ProviderTileLayer* p = inner_;
    inner_ = nullptr;
    return p;
  }

  bool Create() override { return inner_ && inner_->Create(); }
  bool Open(const char* szLayerArchiveName) override {
    return inner_ && inner_->Open(szLayerArchiveName);
  }
  bool Close() override { return inner_ && inner_->Close(); }
  bool Fetch(FetchType type = FETCH_ALL) override {
    (void)type;
    return inner_ && inner_->Fetch();
  }
  void CalEnvelope() override {
    if (inner_) {
      inner_->CalEnvelope();
      inner_->get_envelope(m_lyrEnv);
      rebuild_tiles();
    }
  }

  int GetTileCount() const override {
    return inner_ ? inner_->GetTileCount() : 0;
  }
  void MoveFirst() const override { cursor_ = 0; }
  void MoveNext() const override { ++cursor_; }
  void MoveLast() const override {
    cursor_ = GetTileCount() > 0 ? GetTileCount() - 1 : 0;
  }
  void Delete() override {}
  bool IsEnd() const override { return cursor_ >= GetTileCount(); }
  void DeleteAll() override {}

  long AppendTile(const SmtTile* /*pTile*/, bool /*bClone*/ = false) override {
    return SMT_ERR_FAILURE;
  }
  long UpdateTile(const SmtTile* /*pTile*/) override { return SMT_ERR_FAILURE; }
  long DeleteTile(const SmtTile* /*pTile*/) override { return SMT_ERR_FAILURE; }
  SmtTile* GetTile() const override { return GetTile(cursor_); }
  SmtTile* GetTile(int index) const override {
    const_cast<LeftoverProviderTileLayer*>(this)->rebuild_tiles();
    if (index < 0 || index >= static_cast<int>(cache_.size())) {
      return nullptr;
    }
    return cache_[static_cast<size_t>(index)];
  }
  SmtTile* GetTileByID(uint unID) const override {
    const_cast<LeftoverProviderTileLayer*>(this)->rebuild_tiles();
    for (SmtTile* t : cache_) {
      if (t && static_cast<uint>(t->lID) == unID) {
        return t;
      }
    }
    return nullptr;
  }

 private:
  void clear_cache() {
    for (SmtTile* t : cache_) {
      if (t) {
        delete[] t->pTileBuf;
        delete t;
      }
    }
    cache_.clear();
  }

  void rebuild_tiles() {
    clear_cache();
    if (!inner_) {
      return;
    }
    const int n = inner_->GetTileCount();
    cache_.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
      const tile::TileImage* img = inner_->GetTile(i);
      auto* t = new SmtTile();
      t->lID = i + 1;
      if (img) {
        t->rtTileRect.lb.x = static_cast<float>(img->world_rect.MinX);
        t->rtTileRect.lb.y = static_cast<float>(img->world_rect.MinY);
        t->rtTileRect.rt.x = static_cast<float>(img->world_rect.MaxX);
        t->rtTileRect.rt.y = static_cast<float>(img->world_rect.MaxY);
        t->lImageCode = img->image_code;
        if (!img->bytes.empty()) {
          t->lTileBufSize = static_cast<long>(img->bytes.size());
          t->pTileBuf = new char[img->bytes.size()];
          std::memcpy(t->pTileBuf, img->bytes.data(), img->bytes.size());
        }
      }
      cache_.push_back(t);
    }
  }

  tile::ProviderTileLayer* inner_ = nullptr;
  bool owns_ = false;
  mutable int cursor_ = 0;
  std::vector<SmtTile*> cache_;
};

}  // namespace gis

#endif  // SMT_LEGACY_GIS_LAYER_TILE_WRAP_H_
