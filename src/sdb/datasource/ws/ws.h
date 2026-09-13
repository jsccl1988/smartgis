// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef _SDE_WS_H
#define _SDE_WS_H

#include "sdb/datasource/ws/sde_ws_export.h"
#include "sdb/layer/layer.h"

#include <string>
#include <vector>

using namespace sdb;

#define MAX_LAYER_URL (MAX_FILE_PATH)

const std::string C_STR_SDE_WSDEVICE_LOG = "SmtSDEWSDevice";

namespace sdb {

// Leftover tile URL device. Not a SmtDataSource and not an OGR path (v1).
class SmtWSTileLayer : public SmtTileLayer {
 public:
  SmtWSTileLayer();
  ~SmtWSTileLayer() override;

  bool Create() override;
  bool Open(const char* szLayerArchiveName) override;
  bool Close() override;
  bool Fetch(eSmtFetchType type = FETCH_ALL) override;
  void CalEnvelope() override;

  int GetTileCount() const override { return m_pMemLayer->GetTileCount(); }
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
  void SetLayerRect(const fRect& lyrRect) override;

 protected:
  char m_szLayerURL[MAX_LAYER_URL];
  SmtTileLayer* m_pMemLayer;
};

class SDE_WS_EXPORT SmtWSDataSource {
 public:
  SmtWSDataSource();
  ~SmtWSDataSource();

  bool Create();
  bool Open();
  bool Close();

  void SetInfo(const SmtDataSourceInfo& info) { info_ = info; }
  void GetInfo(SmtDataSourceInfo& info) const { info = info_; }

  SmtTileLayer* CreateTileLayer(const char* szName, fRect& lyrRect,
                                long lImageCode);
  SmtTileLayer* OpenTileLayer(const char* szName);
  bool DeleteTileLayer(const char* szName);

 private:
  SmtDataSourceInfo info_;
  bool open_ = false;
  std::vector<SmtLayerInfo> layers_;
};

}  // namespace sdb

#endif  // _SDE_WS_H
