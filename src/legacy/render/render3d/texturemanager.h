/*
File:    rd3d_texturemanager.h

Desc:     ��ά ����������

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _RD3D_TEXTUREMANAGER_H
#define _RD3D_TEXTUREMANAGER_H

#include <map>

#include "legacy/render/render3d/texture.h"

using namespace base;
using namespace render;

namespace render {
typedef vector<SmtTexture*> vTexturePtrs;
typedef map<string, SmtTexture*> mapNameToTexturePtrs;
typedef pair<string, SmtTexture*> pairNameToTexturePtr;

class RENDER3D_EXPORT_CLASS SmtTextureManager {
 public:
  SmtTextureManager(void);
  virtual ~SmtTextureManager(void);

 public:
  //	static SmtTexture*				CreateTexture(LP3DRENDERDEVICE
  // pRenderDevice,SmtFileInfo &info,const string strName);

 public:
  long AddTexture(SmtTexture* pTexture);
  SmtTexture* GetTexture(const char* szName);
  long DestroyTexture(const char* szName);
  long DestroyAllTexture(void);

  void GetAllTextureName(vector<string>& vStrAllTextureName);

 private:
  mapNameToTexturePtrs m_mapNameToTexturePtrs;
};
}  // namespace render

#if !defined(RENDER3D_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "legacy_render_d.lib")
#else
#pragma comment(lib, "legacy_render.lib")
#endif
#endif

#endif  //_RD3D_TEXTUREMANAGER_H