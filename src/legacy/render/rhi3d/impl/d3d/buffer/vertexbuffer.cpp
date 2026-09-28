// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi3d/impl/d3d/buffer/vertexbuffer.h"

namespace render {

SmtD3DVertexBuffer::SmtD3DVertexBuffer(int count, ulong format, bool isDynamic)
    : locked_(false),
      dynamic_(isDynamic),
      vertex_count_(static_cast<ulong>(count)),
      format_(format),
      stride_(0),
      coord_num_(0),
      cur_vertex_(nullptr),
      cur_color_(nullptr),
      cur_normal_(nullptr),
      cur_texcoord_(nullptr),
      positions_(nullptr),
      colors_(nullptr),
      normals_(nullptr),
      texcoords_(nullptr) {
  ulong size = 0;
  if (format & VF_XYZ) {
    coord_num_ = 3;
    size += sizeof(float) * 3;
  } else if (format & VF_XYZRHW) {
    coord_num_ = 4;
    size += sizeof(float) * 4;
  }
  if (format & VF_NORMAL) size += sizeof(float) * 3;
  if (format & VF_DIFFUSE) size += sizeof(ulong);
  if (format & VF_TEXCOORD) size += sizeof(float) * 2;
  stride_ = size;

  if ((format_ & VF_XYZ) || (format_ & VF_XYZRHW))
    positions_ = new float[coord_num_ * vertex_count_];
  if (format_ & VF_NORMAL) normals_ = new float[3 * vertex_count_];
  if (format_ & VF_DIFFUSE) colors_ = new float[4 * vertex_count_];
  if (format_ & VF_TEXCOORD) texcoords_ = new float[2 * vertex_count_];
}

SmtD3DVertexBuffer::~SmtD3DVertexBuffer() {
  delete[] positions_;
  delete[] normals_;
  delete[] colors_;
  delete[] texcoords_;
}

long SmtD3DVertexBuffer::Lock() {
  locked_ = true;
  cur_vertex_ = positions_;
  cur_color_ = colors_;
  cur_normal_ = normals_;
  cur_texcoord_ = texcoords_;
  return SMT_ERR_NONE;
}

long SmtD3DVertexBuffer::Unlock() {
  locked_ = false;
  cur_vertex_ = nullptr;
  cur_color_ = nullptr;
  cur_normal_ = nullptr;
  cur_texcoord_ = nullptr;
  return SMT_ERR_NONE;
}

void* SmtD3DVertexBuffer::GetVertexData() { return positions_; }

void SmtD3DVertexBuffer::Vertex(float x, float y, float z) {
  cur_vertex_[0] = x;
  cur_vertex_[1] = y;
  cur_vertex_[2] = z;
  cur_vertex_ += 3;
}

void SmtD3DVertexBuffer::Vertex(float x, float y, float z, float w) {
  cur_vertex_[0] = x;
  cur_vertex_[1] = y;
  cur_vertex_[2] = z;
  cur_vertex_[3] = w;
  cur_vertex_ += 4;
}

void SmtD3DVertexBuffer::Normal(float x, float y, float z) {
  cur_normal_[0] = x;
  cur_normal_[1] = y;
  cur_normal_[2] = z;
  cur_normal_ += 3;
}

void SmtD3DVertexBuffer::Diffuse(float r, float g, float b, float a) {
  cur_color_[0] = r;
  cur_color_[1] = g;
  cur_color_[2] = b;
  cur_color_[3] = a;
  cur_color_ += 4;
}

void SmtD3DVertexBuffer::TexVertex(float u, float v) {
  cur_texcoord_[0] = u;
  cur_texcoord_[1] = v;
  cur_texcoord_ += 2;
}

long SmtD3DVertexBuffer::PrepareForDrawing() { return SMT_ERR_NONE; }

long SmtD3DVertexBuffer::EndDrawing() { return SMT_ERR_NONE; }

}  // namespace render
