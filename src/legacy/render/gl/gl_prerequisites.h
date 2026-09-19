/*
File:    gl_prerequisites.h

Desc:

Version: Version 1.0

Writter:  �´���

Date:    2011.2.27

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GL_PREREQUISITES_H
#define _GL_PREREQUISITES_H

#include "legacy/render/render3d/base.h"

#if defined(RENDER_GL_EXPORTS)
#define RENDER_GL_EXPORT_API __declspec(dllexport)
#define RENDER_GL_EXPORT_CLASS __declspec(dllexport)
#else
#define RENDER_GL_EXPORT_API __declspec(dllimport)
#define RENDER_GL_EXPORT_CLASS __declspec(dllimport)
#endif

#include <GL/gl.h>
#include <GL/glu.h>

#include "GL/glext.h"
#include "GL/wglext.h"

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")

#endif  //_GL_PREREQUISITES_H