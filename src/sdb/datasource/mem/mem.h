// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef _SDE_MEM_H
#define _SDE_MEM_H

#include "sdb/datasource/mem/sde_mem_export.h"
#include "sdb/layer/layer.h"

using namespace sdb;

namespace sdb {

// In-process tile list. Raster scratch uses OgrRasterLayer + GDAL MEM.
class SDE_MEM_EXPORT SmtMemTileLayer : public SmtTileLayer {
 public:
  SmtMemTileLayer();
  ~SmtMemTileLayer() override;

  bool Create() override;
  bool Open(const char* szLayerArchiveName) override;
  bool Close() override;
  bool Fetch(eSmtFetchType type = FETCH_ALL) override;
  void CalEnvelope() override;

  int GetTileCount() const override { return static_cast<int>(m_vTilePtrs.size()); }
  void MoveFirst() const override;
  void MoveNext() const override;
  void MoveLast() const override;
  void Delete() override;
  bool IsEnd() const override;
  void DeleteAll() override;

  long AppendTile(const SmtTile* pTile, bool bClone = false) override;
  long UpdateTile(const SmtTile* pTile) override;
  long DeleteTile(const SmtTile* pTile) override;
  SmtTile* GetTile() const override;
  SmtTile* GetTile(int index) const override;
  SmtTile* GetTileByID(uint unID) const override;

 protected:
  vector<SmtTile*> m_vTilePtrs;
  mutable int m_nIteratorIndex;
};

}  // namespace sdb

#endif  // _SDE_MEM_H
