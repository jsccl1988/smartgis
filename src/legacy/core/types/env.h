// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _SMT_ENV_STRUCT_H
#define _SMT_ENV_STRUCT_H

#include "legacy/core/macros/macros.h"

#define MAX_STYLENAME_LENGTH MAX_NAME_LENGTH
#define MAX_MAPNAME_LENGTH MAX_NAME_LENGTH

#define SmtPrjHead (SmartVersion + 0x0001)

namespace base {
struct SmtStyleConfig {
  char szPointStyle[MAX_STYLENAME_LENGTH];
  char szLineStyle[MAX_STYLENAME_LENGTH];
  char szRegionStyle[MAX_STYLENAME_LENGTH];
  char szAuxStyle[MAX_STYLENAME_LENGTH];

  char szDotFlashStyle1[MAX_STYLENAME_LENGTH];
  char szDotFlashStyle2[MAX_STYLENAME_LENGTH];
  char szLineFlashStyle1[MAX_STYLENAME_LENGTH];
  char szLineFlashStyle2[MAX_STYLENAME_LENGTH];
  char szRegionFlashStyle1[MAX_STYLENAME_LENGTH];
  char szRegionFlashStyle2[MAX_STYLENAME_LENGTH];

  SmtStyleConfig() {
    sprintf(szPointStyle, "DefPointStyle");
    sprintf(szLineStyle, "DefLineStyle");
    sprintf(szRegionStyle, "DefRegionStyle");
    sprintf(szAuxStyle, "DefAuxStyle");

    // flash style
    sprintf(szDotFlashStyle1, "DefAnnoFlashStyle1");
    sprintf(szDotFlashStyle2, "DefAnnoFlashStyle2");
    sprintf(szLineFlashStyle1, "DefLineFlashStyle1");
    sprintf(szLineFlashStyle2, "DefLineFlashStyle2");
    sprintf(szRegionFlashStyle1, "DefRegionFlashStyle1");
    sprintf(szRegionFlashStyle2, "DefRegionFlashStyle2");
  }
};

struct MapDocInfo {
  char szMapName[MAX_MAPNAME_LENGTH];
  MapDocInfo() { sprintf(szMapName, "DefMap"); }
};

struct SmtPrjInfo {
  int head;
  MapDocInfo mapDocInfo;
  char szMapDocPath[MAX_PATH];
};

struct SmtFlashPra {
  long lClr1;
  long lClr2;
  long lElapse;
  SmtFlashPra() {
    lClr1 = RGB(255, 0, 0);
    lClr2 = RGB(0, 255, 0);
    lElapse = 100;
  }
};

struct SmtSysPra {
  SmtFlashPra flashPra;
  float fSmargin;
  float fZoomScaleDelt;
  bool bShowMBR;
  bool bShowPoint;
  long lPointRaduis;
  long l2DViewRefreshTime;
  long l2DViewNotifyTime;
  long l3DViewClearColor;
  long l3DViewRefreshTime;
  long l3DViewNotifyTime;
  string str2DRenderDeviceName;
  string str3DRenderDeviceName;

  SmtSysPra()
      : fSmargin(.5),
        fZoomScaleDelt(0.25),
        bShowMBR(true),
        bShowPoint(true),
        lPointRaduis(4),
        l2DViewRefreshTime(500),
        l2DViewNotifyTime(50),
        l3DViewRefreshTime(50),
        l3DViewNotifyTime(50),
        str2DRenderDeviceName("SmtGdiRenderDevice")  // SmtGdiRenderDevice
        ,
        str3DRenderDeviceName("Direct3D")  // Direct3D (default), OpenGL
  {
    l3DViewClearColor = RGB(0, 0, 0);
  }
};
}  // namespace base

#endif  // _SMT_ENV_STRUCT_H