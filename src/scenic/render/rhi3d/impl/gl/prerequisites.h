// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _GL_PREREQUISITES_H
#define _GL_PREREQUISITES_H

// WINGDIAPI / APIENTRY come from windows.h; GL/gl.h requires them first.
#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include "GL/glext.h"
#include "GL/wglext.h"
#include "scenic/render/rhi3d/public/device/base.h"

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")

#endif  //_GL_PREREQUISITES_H