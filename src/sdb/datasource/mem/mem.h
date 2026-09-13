// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef _SDE_MEM_H
#define _SDE_MEM_H

#include "layer.h"

#if defined(Export_SmtSDEMemDevice)
#define SMT_SDE_MEM_EXPORT __declspec(dllexport)
#else
#define SMT_SDE_MEM_EXPORT __declspec(dllimport)
#endif

using namespace Smt_GIS;

namespace Smt_SDEMem {

// In-process raster buffer. Vector scratch uses the GDAL Memory driver.
class SMT_SDE_MEM_EXPORT SmtMemRasLayer : public SmtRasterLayer {
 public:
  SmtMemRasLayer();
  ~SmtMemRasLayer() override;

  bool Create() override;
  bool Open(const char* szLayerArchiveName) override;
  bool Close() override;
  bool Fetch(eSmtFetchType type = FETCH_ALL) override;
  void CalEnvelope() override;

  long CreaterRaster(const char* pRasterBuf, long lRasterBufSize,
                     const fRect& fRasterRect, long lImageCode) override;
  long SetRasterRect(const fRect& fLocRect) override;
  long GetRaster(char*& pRasterBuf, long& lRasterBufSize, fRect& fRasterRect,
                 long& lImageCode) const override;
  long GetRasterNoClone(char*& pRasterBuf, long& lRasterBufSize,
                        fRect& fLocRect, long& lImageCode) const override;
  long GetRasterRect(fRect& fLocRect) const override;

 protected:
  fRect m_fRasterRect;
  char* m_pRasterBuf;
  long m_lRasterBufSize;
  long m_lCodeType;
};

class SMT_SDE_MEM_EXPORT SmtMemTileLayer : public SmtTileLayer {
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

}  // namespace Smt_SDEMem

#if !defined(Export_SmtSDEMemDevice)
#if defined(_DEBUG)
#pragma comment(lib, "SmtSDEMemDeviceD.lib")
#else
#pragma comment(lib, "SmtSDEMemDevice.lib")
#endif
#endif

#endif  // _SDE_MEM_H
