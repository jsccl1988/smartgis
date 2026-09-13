#ifndef PLUGIN_ORTHOGRID_PLUG_H_
#define PLUGIN_ORTHOGRID_PLUG_H_

#include "plugin/orthogrid/grid.h"
#include "plugin/module.h"
#include "ui/xview/view_2d_edit.h"

#include "ogr_geometry.h"

using namespace plugin;
using namespace orthogrid;
using namespace ui;

class OrthogridPlugin : public SmtAuxModule {
 public:
  OrthogridPlugin(void);
  virtual ~OrthogridPlugin(void);

 public:
  int Init(void);
  int Destroy(void);

 public:
  int notify(long lMsg, SmtListenerMsg& param);

 protected:
  int OnInputBnd0(void);
  int OnInputBnd2(void);

  void LoadFromFile(void);

 protected:
  int Init2DStuff(void);

  int CreateIAGetLineTool(void);
  static int GetIAToolResult(long nMsg, SmtListenerMsg& param);

 public:
  int OnEndInputBnd(OGRLineString* pLineString);

 public:
  SmtBaseTool* m_pActiveTool;

  vdbfPoints m_ctrlBnd0;
  vdbfPoints m_ctrlBnd2;
  int m_bndIndex;

 protected:
  LPRENDERDEVICE m_pRenderDevice;
  Smt2DEditXView* m_p2DEditView;
};

#endif  // PLUGIN_ORTHOGRID_PLUG_H_
