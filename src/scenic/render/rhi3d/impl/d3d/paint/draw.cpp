// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <d3dcompiler.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "scenic/render/rhi3d/impl/d3d/host/render_device.h"
#include "scenic/render/rhi3d/impl/d3d/resource/buffer/index_buffer.h"
#include "scenic/render/rhi3d/impl/d3d/resource/buffer/vertex_buffer.h"

#pragma comment(lib, "d3dcompiler.lib")

namespace scenic {
namespace detail {
namespace {

constexpr const char* kMeshHlsl = R"(
cbuffer Frame : register(b0) {
  row_major float4x4 g_mvp;
  float g_use_tex;
  float3 g_pad;
};
struct VSIn {
  float3 pos : POSITION;
  float3 nrm : NORMAL;
  float4 col : COLOR;
  float2 uv : TEXCOORD0;
};
struct VSOut {
  float4 pos : SV_POSITION;
  float3 nrm : NORMAL;
  float4 col : COLOR;
  float2 uv : TEXCOORD0;
};
VSOut VSMain(VSIn i) {
  VSOut o;
  o.pos = mul(float4(i.pos, 1.0f), g_mvp);
  o.nrm = i.nrm;
  o.col = i.col;
  o.uv = i.uv;
  return o;
}
Texture2D g_tex : register(t0);
SamplerState g_samp : register(s0);
float4 PSMain(VSOut i) : SV_TARGET {
  // Match leftover GL COLOR_MATERIAL with a soft key light.
  float3 n = normalize(i.nrm);
  float3 L = normalize(float3(0.35f, 0.85f, 0.40f));
  float ndl = saturate(dot(n, L));
  float3 base = i.col.rgb;
  if (g_use_tex > 0.5f) {
    base *= g_tex.Sample(g_samp, i.uv).rgb;
  }
  float3 rgb = base * (0.42f + 0.48f * ndl) + float3(0.10f, 0.11f, 0.12f);
  return float4(saturate(rgb), 1.0f);
}
)";

constexpr const char* kSpriteHlsl = R"(
cbuffer Frame : register(b0) {
  float2 g_inv_vp;
  float2 g_pad;
};
struct VSIn {
  float2 pos : POSITION;
  float2 uv : TEXCOORD0;
};
struct VSOut {
  float4 pos : SV_POSITION;
  float2 uv : TEXCOORD0;
};
VSOut VSMain(VSIn i) {
  VSOut o;
  float x = i.pos.x * g_inv_vp.x * 2.0f - 1.0f;
  float y = 1.0f - i.pos.y * g_inv_vp.y * 2.0f;
  o.pos = float4(x, y, 0.0f, 1.0f);
  o.uv = i.uv;
  return o;
}
Texture2D g_tex : register(t0);
SamplerState g_samp : register(s0);
float4 PSMain(VSOut i) : SV_TARGET {
  return g_tex.Sample(g_samp, i.uv);
}
)";

struct MeshCb {
  float mvp[16];
  float use_tex;
  float pad[3];
};

struct MeshVertex {
  float px, py, pz;
  float nx, ny, nz;
  float r, g, b, a;
  float u, v;
};

struct SpriteCb {
  float inv_vp[2];
  float pad[2];
};

struct SpriteVertex {
  float x, y;
  float u, v;
};

void copy_matrix_row_major(float* dst, const Matrix& m) {
  dst[0] = m._11;
  dst[1] = m._12;
  dst[2] = m._13;
  dst[3] = m._14;
  dst[4] = m._21;
  dst[5] = m._22;
  dst[6] = m._23;
  dst[7] = m._24;
  dst[8] = m._31;
  dst[9] = m._32;
  dst[10] = m._33;
  dst[11] = m._34;
  dst[12] = m._41;
  dst[13] = m._42;
  dst[14] = m._43;
  dst[15] = m._44;
}

bool fill_mesh_verts(D3dVertexBuffer* dvb, std::vector<MeshVertex>* out) {
  if (!dvb || !out || !dvb->positions()) {
    return false;
  }
  const ulong format = dvb->GetVertexFormat();
  if ((format & VF_XYZ) == 0) {
    return false;
  }
  const ulong nvert = dvb->GetVertexCount();
  if (nvert == 0) {
    return false;
  }
  out->resize(static_cast<size_t>(nvert));
  const float* pos = dvb->positions();
  const float* nrm = dvb->normals();
  const float* col = dvb->colors();
  const float* uv = dvb->texcoords();
  for (ulong i = 0; i < nvert; ++i) {
    MeshVertex& v = (*out)[static_cast<size_t>(i)];
    v.px = pos[i * 3 + 0];
    v.py = pos[i * 3 + 1];
    v.pz = pos[i * 3 + 2];
    if (nrm) {
      v.nx = nrm[i * 3 + 0];
      v.ny = nrm[i * 3 + 1];
      v.nz = nrm[i * 3 + 2];
    } else {
      v.nx = 0.f;
      v.ny = 1.f;
      v.nz = 0.f;
    }
    if (col) {
      v.r = col[i * 4 + 0];
      v.g = col[i * 4 + 1];
      v.b = col[i * 4 + 2];
      v.a = col[i * 4 + 3];
    } else {
      v.r = v.g = v.b = 0.7f;
      v.a = 1.f;
    }
    if (uv && (format & VF_TEXCOORD)) {
      v.u = uv[i * 2 + 0];
      v.v = uv[i * 2 + 1];
    } else {
      v.u = 0.f;
      v.v = 0.f;
    }
  }
  return true;
}

D3D11_PRIMITIVE_TOPOLOGY topology_for(PrimitiveType type) {
  switch (type) {
    case PT_POINTLIST:
      return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
    case PT_LINELIST:
      return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
    case PT_LINESTRIP:
      return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
    case PT_TRIANGLELIST:
      return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    case PT_TRIANGLESTRIP:
      return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    default:
      return D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
  }
}

// Match leftover GL GetOpenGLPrimitiveType vertex/index counts.
ulong draw_vertex_count(PrimitiveType type, ulong primitive_count) {
  switch (type) {
    case PT_POINTLIST:
    case PT_LINESTRIP:
      return primitive_count;
    case PT_LINELIST:
      return primitive_count * 2;
    case PT_TRIANGLELIST:
      return primitive_count * 3;
    case PT_TRIANGLESTRIP:
    case PT_TRIANGLEFAN:
      return primitive_count + 2;
    default:
      return primitive_count;
  }
}

void bind_mesh_ps_resources(ID3D11DeviceContext* ctx, MeshCb* cb, bool use_tex,
                            ID3D11ShaderResourceView* srv,
                            ID3D11SamplerState* samp) {
  cb->use_tex = use_tex ? 1.f : 0.f;
  cb->pad[0] = cb->pad[1] = cb->pad[2] = 0.f;
  ID3D11ShaderResourceView* bound_srv = srv;
  ctx->PSSetShaderResources(0, 1, &bound_srv);
  if (samp) {
    ctx->PSSetSamplers(0, 1, &samp);
  }
}

}  // namespace

void D3dRenderDevice::release_mesh_pipeline() {
  safe_release(mesh_vs_);
  safe_release(mesh_ps_);
  safe_release(mesh_il_);
  safe_release(mesh_cb_);
  safe_release(mesh_rs_);
  safe_release(mesh_dss_);
  safe_release(mesh_bs_);
  mesh_pipeline_ok_ = false;
}

namespace {

template <typename T>
void release_com(T*& ptr) {
  if (ptr) {
    ptr->Release();
    ptr = nullptr;
  }
}

// Sidecar keeps D3dRenderDevice ABI/size stable across partial rebuilds.
struct SpriteGpuTex {
  // MapLabelBatch keeps RasterCache::bgra stable — key by pointer, not pixels.
  // Full-buffer FNV every draw was the D3D vs GL gap on Debug china (~30 labels).
  const void* pixels = nullptr;
  UINT w = 0;
  UINT h = 0;
  ID3D11Texture2D* tex = nullptr;
  ID3D11ShaderResourceView* srv = nullptr;
};

struct SpritePipe {
  ID3D11VertexShader* vs = nullptr;
  ID3D11PixelShader* ps = nullptr;
  ID3D11InputLayout* il = nullptr;
  ID3D11Buffer* cb = nullptr;
  ID3D11Buffer* dynamic_vb = nullptr;  // reused quad (DYNAMIC)
  ID3D11SamplerState* samp = nullptr;
  ID3D11BlendState* bs = nullptr;
  ID3D11DepthStencilState* dss = nullptr;
  ID3D11RasterizerState* rs = nullptr;
  // Stable label rasters (MapLabelBatch) — avoid CreateTexture2D every Present.
  std::unordered_map<const void*, SpriteGpuTex> tex_by_ptr;
  std::vector<const void*> tex_lru;
  static constexpr size_t kTexCacheCap = 128;
  bool ok = false;

  void release_tex_cache() {
    for (auto& kv : tex_by_ptr) {
      release_com(kv.second.srv);
      release_com(kv.second.tex);
    }
    tex_by_ptr.clear();
    tex_lru.clear();
  }

  void release() {
    release_tex_cache();
    release_com(vs);
    release_com(ps);
    release_com(il);
    release_com(cb);
    release_com(dynamic_vb);
    release_com(samp);
    release_com(bs);
    release_com(dss);
    release_com(rs);
    ok = false;
  }
};

std::mutex g_sprite_mu;
std::unordered_map<D3dRenderDevice*, SpritePipe> g_sprites;

SpritePipe* sprite_for(D3dRenderDevice* device) {
  std::lock_guard<std::mutex> lock(g_sprite_mu);
  return &g_sprites[device];
}

long ensure_sprite_pipe(D3dRenderDevice* device, ID3D11Device* d3d,
                        SpritePipe* pipe) {
  if (!device || !d3d || !pipe) {
    return SMT_ERR_FAILURE;
  }
  if (pipe->ok) {
    return SMT_ERR_NONE;
  }

  ID3DBlob* vs_blob = nullptr;
  ID3DBlob* ps_blob = nullptr;
  ID3DBlob* err = nullptr;
  HRESULT hr =
      D3DCompile(kSpriteHlsl, std::strlen(kSpriteHlsl), "leftover_sprite",
                 nullptr, nullptr, "VSMain", "vs_4_0", 0, 0, &vs_blob, &err);
  if (FAILED(hr) || !vs_blob) {
    release_com(err);
    return SMT_ERR_FAILURE;
  }
  release_com(err);
  hr = D3DCompile(kSpriteHlsl, std::strlen(kSpriteHlsl), "leftover_sprite",
                  nullptr, nullptr, "PSMain", "ps_4_0", 0, 0, &ps_blob, &err);
  if (FAILED(hr) || !ps_blob) {
    release_com(vs_blob);
    release_com(err);
    return SMT_ERR_FAILURE;
  }
  release_com(err);

  hr = d3d->CreateVertexShader(vs_blob->GetBufferPointer(),
                               vs_blob->GetBufferSize(), nullptr, &pipe->vs);
  if (FAILED(hr)) {
    release_com(vs_blob);
    release_com(ps_blob);
    return SMT_ERR_FAILURE;
  }
  hr = d3d->CreatePixelShader(ps_blob->GetBufferPointer(),
                              ps_blob->GetBufferSize(), nullptr, &pipe->ps);
  if (FAILED(hr)) {
    release_com(vs_blob);
    release_com(ps_blob);
    pipe->release();
    return SMT_ERR_FAILURE;
  }

  const D3D11_INPUT_ELEMENT_DESC layout[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
       D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8,
       D3D11_INPUT_PER_VERTEX_DATA, 0},
  };
  hr = d3d->CreateInputLayout(layout, 2, vs_blob->GetBufferPointer(),
                              vs_blob->GetBufferSize(), &pipe->il);
  release_com(vs_blob);
  release_com(ps_blob);
  if (FAILED(hr)) {
    pipe->release();
    return SMT_ERR_FAILURE;
  }

  D3D11_BUFFER_DESC cbd = {};
  cbd.ByteWidth = sizeof(SpriteCb);
  cbd.Usage = D3D11_USAGE_DYNAMIC;
  cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  hr = d3d->CreateBuffer(&cbd, nullptr, &pipe->cb);
  if (FAILED(hr)) {
    pipe->release();
    return SMT_ERR_FAILURE;
  }

  D3D11_SAMPLER_DESC sd = {};
  sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
  sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
  sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
  sd.MaxLOD = D3D11_FLOAT32_MAX;
  hr = d3d->CreateSamplerState(&sd, &pipe->samp);
  if (FAILED(hr)) {
    pipe->release();
    return SMT_ERR_FAILURE;
  }

  D3D11_BLEND_DESC bd = {};
  bd.RenderTarget[0].BlendEnable = TRUE;
  bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
  bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
  bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
  bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
  bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
  bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
  bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
  hr = d3d->CreateBlendState(&bd, &pipe->bs);
  if (FAILED(hr)) {
    pipe->release();
    return SMT_ERR_FAILURE;
  }

  D3D11_DEPTH_STENCIL_DESC dd = {};
  dd.DepthEnable = FALSE;
  dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
  dd.DepthFunc = D3D11_COMPARISON_ALWAYS;
  hr = d3d->CreateDepthStencilState(&dd, &pipe->dss);
  if (FAILED(hr)) {
    pipe->release();
    return SMT_ERR_FAILURE;
  }

  D3D11_RASTERIZER_DESC rd = {};
  rd.FillMode = D3D11_FILL_SOLID;
  rd.CullMode = D3D11_CULL_NONE;
  rd.DepthClipEnable = TRUE;
  hr = d3d->CreateRasterizerState(&rd, &pipe->rs);
  if (FAILED(hr)) {
    pipe->release();
    return SMT_ERR_FAILURE;
  }

  D3D11_BUFFER_DESC vbd = {};
  vbd.ByteWidth = sizeof(SpriteVertex) * 6;
  vbd.Usage = D3D11_USAGE_DYNAMIC;
  vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  hr = d3d->CreateBuffer(&vbd, nullptr, &pipe->dynamic_vb);
  if (FAILED(hr) || !pipe->dynamic_vb) {
    pipe->release();
    return SMT_ERR_FAILURE;
  }

  pipe->ok = true;
  return SMT_ERR_NONE;
}

}  // namespace

void release_sprite_sidecar(D3dRenderDevice* device) {
  if (!device) {
    return;
  }
  std::lock_guard<std::mutex> lock(g_sprite_mu);
  auto it = g_sprites.find(device);
  if (it == g_sprites.end()) {
    return;
  }
  it->second.release();
  g_sprites.erase(it);
}

long D3dRenderDevice::ensure_mesh_pipeline() {
  if (mesh_pipeline_ok_) {
    return SMT_ERR_NONE;
  }
  if (!device_) {
    return SMT_ERR_FAILURE;
  }

  ID3DBlob* vs_blob = nullptr;
  ID3DBlob* ps_blob = nullptr;
  ID3DBlob* err = nullptr;
  HRESULT hr =
      D3DCompile(kMeshHlsl, std::strlen(kMeshHlsl), "leftover_mesh", nullptr,
                 nullptr, "VSMain", "vs_4_0", 0, 0, &vs_blob, &err);
  if (FAILED(hr) || !vs_blob) {
    safe_release(err);
    return SMT_ERR_FAILURE;
  }
  safe_release(err);
  hr = D3DCompile(kMeshHlsl, std::strlen(kMeshHlsl), "leftover_mesh", nullptr,
                  nullptr, "PSMain", "ps_4_0", 0, 0, &ps_blob, &err);
  if (FAILED(hr) || !ps_blob) {
    safe_release(vs_blob);
    safe_release(err);
    return SMT_ERR_FAILURE;
  }
  safe_release(err);

  hr =
      device_->CreateVertexShader(vs_blob->GetBufferPointer(),
                                  vs_blob->GetBufferSize(), nullptr, &mesh_vs_);
  if (FAILED(hr)) {
    safe_release(vs_blob);
    safe_release(ps_blob);
    return SMT_ERR_FAILURE;
  }
  hr = device_->CreatePixelShader(ps_blob->GetBufferPointer(),
                                  ps_blob->GetBufferSize(), nullptr, &mesh_ps_);
  if (FAILED(hr)) {
    safe_release(vs_blob);
    safe_release(ps_blob);
    release_mesh_pipeline();
    return SMT_ERR_FAILURE;
  }

  const D3D11_INPUT_ELEMENT_DESC layout[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
       D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
       D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24,
       D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40,
       D3D11_INPUT_PER_VERTEX_DATA, 0},
  };
  hr = device_->CreateInputLayout(layout, 4, vs_blob->GetBufferPointer(),
                                  vs_blob->GetBufferSize(), &mesh_il_);
  safe_release(vs_blob);
  safe_release(ps_blob);
  if (FAILED(hr)) {
    release_mesh_pipeline();
    return SMT_ERR_FAILURE;
  }

  D3D11_BUFFER_DESC cbd = {};
  cbd.ByteWidth = sizeof(MeshCb);
  cbd.Usage = D3D11_USAGE_DYNAMIC;
  cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  hr = device_->CreateBuffer(&cbd, nullptr, &mesh_cb_);
  if (FAILED(hr)) {
    release_mesh_pipeline();
    return SMT_ERR_FAILURE;
  }

  D3D11_RASTERIZER_DESC rd = {};
  rd.FillMode = D3D11_FILL_SOLID;
  rd.CullMode = D3D11_CULL_NONE;
  rd.DepthClipEnable = TRUE;
  hr = device_->CreateRasterizerState(&rd, &mesh_rs_);
  if (FAILED(hr)) {
    release_mesh_pipeline();
    return SMT_ERR_FAILURE;
  }

  D3D11_DEPTH_STENCIL_DESC dd = {};
  dd.DepthEnable = TRUE;
  dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
  dd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
  hr = device_->CreateDepthStencilState(&dd, &mesh_dss_);
  if (FAILED(hr)) {
    release_mesh_pipeline();
    return SMT_ERR_FAILURE;
  }

  D3D11_BLEND_DESC bd = {};
  bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
  hr = device_->CreateBlendState(&bd, &mesh_bs_);
  if (FAILED(hr)) {
    release_mesh_pipeline();
    return SMT_ERR_FAILURE;
  }

  mesh_pipeline_ok_ = true;
  return SMT_ERR_NONE;
}

long D3dRenderDevice::DrawPrimitives(PrimitiveType type,
                                        VertexBuffer* pVB, ulong baseVertex,
                                        ulong primitiveCount) {
  if (!pVB || !active_context() || !device_ || primitiveCount == 0) {
    return SMT_ERR_INVALID_PARAM;
  }
  const D3D11_PRIMITIVE_TOPOLOGY topo = topology_for(type);
  if (topo == D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED) {
    return SMT_ERR_FAILURE;
  }
  if (ensure_mesh_pipeline() != SMT_ERR_NONE) {
    return SMT_ERR_FAILURE;
  }

  auto* dvb = static_cast<D3dVertexBuffer*>(pVB);
  if (!dvb) {
    return SMT_ERR_FAILURE;
  }
  const ulong want = draw_vertex_count(type, primitiveCount);
  std::vector<MeshVertex> verts;
  ID3D11Buffer* vb = dvb->ensure_gpu_vb(device_, nullptr, 0);
  if (!vb) {
    if (!fill_mesh_verts(dvb, &verts)) {
      return SMT_ERR_FAILURE;
    }
    if (baseVertex >= verts.size() ||
        baseVertex + want > static_cast<ulong>(verts.size())) {
      return SMT_ERR_FAILURE;
    }
    vb = dvb->ensure_gpu_vb(
        device_, verts.data(),
        static_cast<UINT>(verts.size() * sizeof(MeshVertex)));
  }
  if (!vb) {
    return SMT_ERR_FAILURE;
  }
  // When cached, still validate base+count against CPU vertex_count.
  if (baseVertex + want > dvb->GetVertexCount()) {
    return SMT_ERR_FAILURE;
  }

  ID3D11DeviceContext* ctx = active_context();
  Matrix gl_to_d3d;
  gl_to_d3d.identity();
  gl_to_d3d._33 = 0.5f;
  gl_to_d3d._43 = 0.5f;
  const Matrix mvp = modelview_ * projection_ * gl_to_d3d;
  float mvp_f[16];
  copy_matrix_row_major(mvp_f, mvp);
  const bool use_tex = bound_texture_ != nullptr;
  const bool mvp_changed =
      !mesh_cb_valid_ || last_mesh_use_tex_ != use_tex ||
      std::memcmp(mvp_f, last_mesh_mvp_, sizeof(mvp_f)) != 0;
  if (mvp_changed) {
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(ctx->Map(active_mesh_cb(), 0, D3D11_MAP_WRITE_DISCARD, 0,
                        &mapped))) {
      return SMT_ERR_FAILURE;
    }
    auto* cb = static_cast<MeshCb*>(mapped.pData);
    std::memcpy(cb->mvp, mvp_f, sizeof(mvp_f));
    ID3D11ShaderResourceView* tex_srv =
        use_tex ? texture_srv(bound_texture_->GetHandle()) : nullptr;
    ID3D11SamplerState* tex_samp = use_tex ? linear_sampler() : nullptr;
    bind_mesh_ps_resources(ctx, cb, use_tex, tex_srv, tex_samp);
    ctx->Unmap(active_mesh_cb(), 0);
    std::memcpy(last_mesh_mvp_, mvp_f, sizeof(mvp_f));
    last_mesh_use_tex_ = use_tex;
    mesh_cb_valid_ = true;
  }

  const UINT stride = sizeof(MeshVertex);
  const UINT offset = static_cast<UINT>(baseVertex * sizeof(MeshVertex));
  // Bind mesh PSO once per burst — 1.7k china line features re-entered here.
  if (!mesh_draw_state_bound_) {
    ctx->IASetInputLayout(mesh_il_);
    ctx->VSSetShader(mesh_vs_, nullptr, 0);
    ctx->PSSetShader(mesh_ps_, nullptr, 0);
    ID3D11Buffer* mesh_cb_bind = active_mesh_cb();
    ctx->VSSetConstantBuffers(0, 1, &mesh_cb_bind);
    ctx->RSSetState(mesh_rs_);
    ctx->OMSetDepthStencilState(mesh_dss_, 0);
    const float blend_factor[4] = {0, 0, 0, 0};
    ctx->OMSetBlendState(mesh_bs_, blend_factor, 0xffffffff);
    mesh_draw_state_bound_ = true;
  }
  ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
  ctx->IASetPrimitiveTopology(topo);
  ctx->Draw(static_cast<UINT>(want), 0);
  return SMT_ERR_NONE;
}

long D3dRenderDevice::DrawIndexedPrimitives(PrimitiveType type,
                                               VertexBuffer* pVB,
                                               IndexBuffer* pIB,
                                               ulong /*baseIndex*/,
                                               ulong primitiveCount) {
  if (!pVB || !pIB || !active_context() || !device_) {
    return SMT_ERR_INVALID_PARAM;
  }
  if (type != PT_TRIANGLELIST || primitiveCount == 0) {
    return SMT_ERR_FAILURE;
  }
  if (ensure_mesh_pipeline() != SMT_ERR_NONE) {
    return SMT_ERR_FAILURE;
  }

  auto* dvb = static_cast<D3dVertexBuffer*>(pVB);
  auto* dib = static_cast<D3dIndexBuffer*>(pIB);
  if (!dvb || !dib || !dib->indices()) {
    return SMT_ERR_FAILURE;
  }

  const ulong nidx = dib->GetIndexCount();
  const ulong want_idx = primitiveCount * 3;
  if (nidx < want_idx) {
    return SMT_ERR_FAILURE;
  }

  // Cache GPU VB/IB across frames. Terrain meshes Unlock once at Create —
  // recreating IMMUTABLE buffers every Present was ~1 FPS.
  std::vector<MeshVertex> verts;
  ID3D11Buffer* vb = dvb->ensure_gpu_vb(device_, nullptr, 0);
  if (!vb) {
    if (!fill_mesh_verts(dvb, &verts)) {
      return SMT_ERR_FAILURE;
    }
    vb = dvb->ensure_gpu_vb(
        device_, verts.data(),
        static_cast<UINT>(verts.size() * sizeof(MeshVertex)));
  }
  if (!vb) {
    return SMT_ERR_FAILURE;
  }

  ID3D11Buffer* ib =
      dib->ensure_gpu_ib(device_, nullptr, 0);
  if (!ib) {
    ib = dib->ensure_gpu_ib(device_, dib->indices(),
                            static_cast<UINT>(want_idx * sizeof(uint)));
  }
  if (!ib) {
    return SMT_ERR_FAILURE;
  }

  ID3D11DeviceContext* ctx = active_context();
  Matrix gl_to_d3d;
  gl_to_d3d.identity();
  gl_to_d3d._33 = 0.5f;
  gl_to_d3d._43 = 0.5f;
  const Matrix mvp = modelview_ * projection_ * gl_to_d3d;
  float mvp_f[16];
  copy_matrix_row_major(mvp_f, mvp);
  const bool use_tex = bound_texture_ != nullptr;
  const bool mvp_changed =
      !mesh_cb_valid_ || last_mesh_use_tex_ != use_tex ||
      std::memcmp(mvp_f, last_mesh_mvp_, sizeof(mvp_f)) != 0;
  if (mvp_changed) {
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(ctx->Map(active_mesh_cb(), 0, D3D11_MAP_WRITE_DISCARD, 0,
                        &mapped))) {
      return SMT_ERR_FAILURE;
    }
    auto* cb = static_cast<MeshCb*>(mapped.pData);
    std::memcpy(cb->mvp, mvp_f, sizeof(mvp_f));
    ID3D11ShaderResourceView* tex_srv =
        use_tex ? texture_srv(bound_texture_->GetHandle()) : nullptr;
    ID3D11SamplerState* tex_samp = use_tex ? linear_sampler() : nullptr;
    bind_mesh_ps_resources(ctx, cb, use_tex, tex_srv, tex_samp);
    ctx->Unmap(active_mesh_cb(), 0);
    std::memcpy(last_mesh_mvp_, mvp_f, sizeof(mvp_f));
    last_mesh_use_tex_ = use_tex;
    mesh_cb_valid_ = true;
  }

  const UINT stride = sizeof(MeshVertex);
  const UINT offset = 0;
  if (!mesh_draw_state_bound_) {
    ctx->IASetInputLayout(mesh_il_);
    ctx->VSSetShader(mesh_vs_, nullptr, 0);
    ctx->PSSetShader(mesh_ps_, nullptr, 0);
    ID3D11Buffer* mesh_cb_bind = active_mesh_cb();
    ctx->VSSetConstantBuffers(0, 1, &mesh_cb_bind);
    ctx->RSSetState(mesh_rs_);
    ctx->OMSetDepthStencilState(mesh_dss_, 0);
    const float blend_factor[4] = {0, 0, 0, 0};
    ctx->OMSetBlendState(mesh_bs_, blend_factor, 0xffffffff);
    mesh_draw_state_bound_ = true;
  }
  ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
  ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);
  ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  ctx->DrawIndexed(static_cast<UINT>(want_idx), 0, 0);
  return SMT_ERR_NONE;
}

long D3dRenderDevice::DrawScreenBgra(float cx, float cy, int w, int h,
                                        const unsigned char* bgra) {
  if (!bgra || w <= 0 || h <= 0 || !device_ || !context_) {
    return SMT_ERR_INVALID_PARAM;
  }
  if (const char* skip = std::getenv("SMT_RHI3D_SKIP_SCREEN_BGRA");
      skip && (skip[0] == '1' || skip[0] == 'y' || skip[0] == 'Y')) {
    return SMT_ERR_NONE;
  }
  SpritePipe* pipe = sprite_for(this);
  if (ensure_sprite_pipe(this, device_, pipe) != SMT_ERR_NONE) {
    return SMT_ERR_FAILURE;
  }
  if (!pipe->dynamic_vb) {
    return SMT_ERR_FAILURE;
  }
  // Sprite PSO replaces mesh binds.
  mesh_draw_state_bound_ = false;
  mesh_cb_valid_ = false;

  const float vw = static_cast<float>(
      m_viewPort.ulWidth > 0 ? m_viewPort.ulWidth : backbuffer_width_);
  const float vh = static_cast<float>(
      m_viewPort.ulHeight > 0 ? m_viewPort.ulHeight : backbuffer_height_);
  if (vw <= 1.f || vh <= 1.f) {
    return SMT_ERR_FAILURE;
  }

  // After P3 ExecuteCommandList the immediate RS viewport can be cleared —
  // rebind so screen-space labels land in the swapchain.
  {
    D3D11_VIEWPORT vp = {};
    vp.TopLeftX = static_cast<float>(m_viewPort.ulX);
    vp.TopLeftY = static_cast<float>(m_viewPort.ulY);
    vp.Width = vw;
    vp.Height = vh;
    vp.MinDepth = 0.f;
    vp.MaxDepth = 1.f;
    context_->RSSetViewports(1, &vp);
  }

  const void* key = bgra;
  ID3D11ShaderResourceView* srv = nullptr;
  auto hit = pipe->tex_by_ptr.find(key);
  if (hit != pipe->tex_by_ptr.end() && hit->second.w == static_cast<UINT>(w) &&
      hit->second.h == static_cast<UINT>(h) && hit->second.srv) {
    srv = hit->second.srv;
    auto& lru = pipe->tex_lru;
    for (size_t i = 0; i + 1 < lru.size(); ++i) {
      if (lru[i] == key) {
        lru.erase(lru.begin() + static_cast<std::ptrdiff_t>(i));
        lru.push_back(key);
        break;
      }
    }
  } else {
    while (pipe->tex_by_ptr.size() >= SpritePipe::kTexCacheCap &&
           !pipe->tex_lru.empty()) {
      const void* old_key = pipe->tex_lru.front();
      pipe->tex_lru.erase(pipe->tex_lru.begin());
      auto it = pipe->tex_by_ptr.find(old_key);
      if (it == pipe->tex_by_ptr.end()) {
        continue;
      }
      release_com(it->second.srv);
      release_com(it->second.tex);
      pipe->tex_by_ptr.erase(it);
    }
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = static_cast<UINT>(w);
    td.Height = static_cast<UINT>(h);
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = bgra;
    init.SysMemPitch = static_cast<UINT>(w * 4);
    ID3D11Texture2D* tex = nullptr;
    if (FAILED(device_->CreateTexture2D(&td, &init, &tex)) || !tex) {
      return SMT_ERR_FAILURE;
    }
    ID3D11ShaderResourceView* created = nullptr;
    if (FAILED(device_->CreateShaderResourceView(tex, nullptr, &created)) ||
        !created) {
      safe_release(tex);
      return SMT_ERR_FAILURE;
    }
    SpriteGpuTex entry;
    entry.pixels = key;
    entry.w = static_cast<UINT>(w);
    entry.h = static_cast<UINT>(h);
    entry.tex = tex;
    entry.srv = created;
    pipe->tex_by_ptr[key] = entry;
    pipe->tex_lru.push_back(key);
    srv = created;
  }

  const float hw = static_cast<float>(w) * 0.5f;
  const float hh = static_cast<float>(h) * 0.5f;
  const SpriteVertex quad[6] = {
      {cx - hw, cy - hh, 0.f, 0.f}, {cx + hw, cy - hh, 1.f, 0.f},
      {cx + hw, cy + hh, 1.f, 1.f}, {cx - hw, cy - hh, 0.f, 0.f},
      {cx + hw, cy + hh, 1.f, 1.f}, {cx - hw, cy + hh, 0.f, 1.f},
  };

  D3D11_MAPPED_SUBRESOURCE mapped = {};
  if (FAILED(context_->Map(pipe->dynamic_vb, 0, D3D11_MAP_WRITE_DISCARD, 0,
                           &mapped))) {
    return SMT_ERR_FAILURE;
  }
  std::memcpy(mapped.pData, quad, sizeof(quad));
  context_->Unmap(pipe->dynamic_vb, 0);

  if (FAILED(context_->Map(pipe->cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
    return SMT_ERR_FAILURE;
  }
  auto* cb = static_cast<SpriteCb*>(mapped.pData);
  cb->inv_vp[0] = 1.f / vw;
  cb->inv_vp[1] = 1.f / vh;
  cb->pad[0] = 0.f;
  cb->pad[1] = 0.f;
  context_->Unmap(pipe->cb, 0);

  const UINT stride = sizeof(SpriteVertex);
  const UINT offset = 0;
  context_->IASetInputLayout(pipe->il);
  context_->IASetVertexBuffers(0, 1, &pipe->dynamic_vb, &stride, &offset);
  context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  context_->VSSetShader(pipe->vs, nullptr, 0);
  context_->PSSetShader(pipe->ps, nullptr, 0);
  context_->VSSetConstantBuffers(0, 1, &pipe->cb);
  context_->PSSetShaderResources(0, 1, &srv);
  context_->PSSetSamplers(0, 1, &pipe->samp);
  context_->RSSetState(pipe->rs);
  context_->OMSetDepthStencilState(pipe->dss, 0);
  const float blend_factor[4] = {0, 0, 0, 0};
  context_->OMSetBlendState(pipe->bs, blend_factor, 0xffffffff);
  context_->Draw(6, 0);

  ID3D11ShaderResourceView* null_srv = nullptr;
  context_->PSSetShaderResources(0, 1, &null_srv);
  return SMT_ERR_NONE;
}

long D3dRenderDevice::DrawText(uint unID, float xpos, float ypos, float zpos,
                                  const Color& color, const char* str, ...) {
  if (!str || unID >= fonts_.size()) {
    return SMT_ERR_INVALID_PARAM;
  }

  char text[256] = {};
  va_list args;
  va_start(args, str);
  std::vsnprintf(text, sizeof(text), str, args);
  va_end(args);

  lPoint screen = {};
  const long xform = Transform3DTo2D(Vector3(xpos, ypos, zpos), screen);
  if (xform != SMT_ERR_NONE) {
    return xform;
  }
  return draw_text_gdi(unID, static_cast<float>(screen.x),
                       static_cast<float>(screen.y), color, text);
}

long D3dRenderDevice::DrawText(uint nID, float xscreen, float yscreen,
                                  const Color& color, const char* str, ...) {
  if (!str || nID >= fonts_.size()) {
    return SMT_ERR_INVALID_PARAM;
  }

  char text[256] = {};
  va_list args;
  va_start(args, str);
  std::vsnprintf(text, sizeof(text), str, args);
  va_end(args);

  return draw_text_gdi(nID, xscreen, yscreen, color, text);
}

long D3dRenderDevice::DrawCube3D(Vector3 /*vCenter*/, float /*fWidth*/,
                                    Color /*smtClr*/) {
  return SMT_ERR_FAILURE;
}

long D3dRenderDevice::CaptureBgr24(unsigned char* out_bgr24, int width_px,
                                      int height_px) {
  if (!out_bgr24 || !device_ || !context_ || !color_tex_ || width_px <= 0 ||
      height_px <= 0) {
    return SMT_ERR_FAILURE;
  }
  // Lazy staging from the offscreen color target (not the DXGI backbuffer).
  // color_tex_ remains valid after DXGI_SWAP_EFFECT_DISCARD Present.
  D3D11_TEXTURE2D_DESC desc = {};
  color_tex_->GetDesc(&desc);
  const bool need_new = !capture_tex_ || desc.Width != backbuffer_width_ ||
                        desc.Height != backbuffer_height_;
  if (need_new) {
    safe_release(capture_tex_);
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.MiscFlags = 0;
    if (FAILED(device_->CreateTexture2D(&desc, nullptr, &capture_tex_))) {
      capture_tex_ = nullptr;
      return SMT_ERR_FAILURE;
    }
  }
  context_->CopyResource(capture_tex_, color_tex_);

  D3D11_MAPPED_SUBRESOURCE mapped = {};
  if (FAILED(context_->Map(capture_tex_, 0, D3D11_MAP_READ, 0, &mapped))) {
    return SMT_ERR_FAILURE;
  }
  const int copy_w = (width_px < static_cast<int>(desc.Width))
                         ? width_px
                         : static_cast<int>(desc.Width);
  const int copy_h = (height_px < static_cast<int>(desc.Height))
                         ? height_px
                         : static_cast<int>(desc.Height);
  std::memset(out_bgr24, 0, static_cast<size_t>(width_px) * height_px * 3u);
  const auto* src = static_cast<const unsigned char*>(mapped.pData);
  // Staging is top-down; BMP/glReadPixels convention is bottom-up.
  for (int y = 0; y < copy_h; ++y) {
    const unsigned char* row = src + static_cast<size_t>(y) * mapped.RowPitch;
    unsigned char* dst = out_bgr24 + static_cast<size_t>(copy_h - 1 - y) *
                                         static_cast<size_t>(width_px) * 3u;
    for (int x = 0; x < copy_w; ++x) {
      dst[x * 3 + 0] = row[x * 4 + 2];  // B
      dst[x * 3 + 1] = row[x * 4 + 1];  // G
      dst[x * 3 + 2] = row[x * 4 + 0];  // R
    }
  }
  context_->Unmap(capture_tex_, 0);
  return SMT_ERR_NONE;
}

}  // namespace detail
}  // namespace scenic
