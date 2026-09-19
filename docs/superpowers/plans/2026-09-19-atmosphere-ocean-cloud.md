<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# ???????? + ??? Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (- [ ]) syntax for tracking.

**Goal:** ? Views ???????? FieldStore?GPU FFT ???????External/Procedural ???????

**Architecture:** gis::atmosphere::Environment ?????FieldStore ??????
ender::atmosphere passes ?? GpuScene::record_draws ????? leftover scene3d?

**Tech Stack:** C++23?GN/uild.bat?GDAL?External ingest??FlyCube/Null RHI?gis::land_mask?

**Spec:** [../specs/2026-09-19-atmosphere-ocean-cloud-design.md](../specs/2026-09-19-atmosphere-ocean-cloud-design.md)

## Global Constraints

- Stay on master??????? commit / ?????  
- ???????gis::atmosphere?
ender::atmosphere??? snake_case?  
- ??????????????copyright 2026 The Mogu Authors?  
- ?? Qt??? SmtScene?Skia ?? 3D ???  
- render tmosphere_sources deps //src/render:rhi_sources?? deps :rhi group??? cycle??  
- ????????FieldChannel / FieldStore / AtmosphereParams / pass ????

---

### Task 0?7: ??? + Field/Ocean/Cloud/FieldTexture/Environment ??

**Steps:** ?? [x]?????????????

---

### Task 8: ????

**Steps:**
1. [x] Spec §5 ????? ColorLoadOp / FlyCube ?? / self-test demo?????????? landed?  
2. [x] ? README.md ????????????????

---

### Task 9: Follow-up ? ?? load-op + self-test demo

**Files:**
- Modify: src/render/rhi/rhi.h?lycube_rhi.cc?ColorLoadOp????????
- Modify: src/render/scene/scene.*?set_color_load_op????? kind ? kLoad?
- Modify: src/render/atmosphere/cloud_pass.*????? + kLoad??????
- Modify: src/app/views/scene3d_controller.cc?main.cc?--self-test ? enable_atmosphere_demo?
- Modify: design / ? plan

**Steps:**
1. [x] Ocean ? land ? clouds ????? kClear ????  
2. [x] FlyCube ???? kClear ??????? solid ?????? wipe?  
3. [x] Views --self-test ? FlyCube present ?? demo?????????  
4. [x] ?????environment_test?scene3d_controller_test?ocean/cloud pass?

---

## Deferred

- [ ] ProceduralStep shallow-water stepper  
- [ ] Full External GRIB/NetCDF driver matrix  
- [ ] leftover scene3d atmosphere migration (won't do)  
- [x] Ocean/cloud HLSL + alpha blend + PipelineId FlyCube PSO  
- [x] **GPU FFT compute**: spectrum + bit-reverse + radix-2 ? height; Null/Gerstner fallback; `SMT_RUN_FLYCUBE_GPU=1`  
- [x] FlyCube multi-pass + depth RT  
- [x] **Ocean FFT quality polish**: directional JONSWAP-lite (+ Phillips fallback), Tessendorf Dx/Dz chop packing (RGBA), analytic Hs energy normalization (no GPU readback)  

### Task 10: RHI hard limits (multi-pass / blend / depth / shaders)

**Steps:**
1. [x] `render::rhi` `PipelineId` / `BlendMode` / `DepthMode` / `DepthLoadOp` / ocean+cloud GPU params  
2. [x] FlyCube multi-pass, depth RT, alpha blend cloud PSO, ocean/cloud HLSL  
3. [x] OceanPass height-tex displace + CloudPass alpha raymarch + Scene3dController  
4. [x] Null stubs + Deferred notes  

### Task 11: GPU FFT compute

**Steps:**
1. [x] `render::rhi` compute: `ComputePipelineId` / `dispatch` / UAV·SRV bind / `supports_compute`  
2. [x] FlyCube `CreateComputePipeline` + spectrum/bitrev/butterfly/encode HLSL  
3. [x] OceanPass GPU path + CPU fallback; Null green  
4. [x] Spec §5 / plan Deferred marked landed (bit-reverse + radix-2; Stockham still optional)

### Task 12: Ocean FFT quality polish

**Steps:**
1. [x] JONSWAP-lite spectrum (wind + ?) with Phillips fallback; CPU `amp_scale` energy norm so ??Hs/4  
2. [x] Tessendorf horizontal displacement Dx/Dz (`kOceanDisplacementSpectrum`) packed into height-map G/B; Ocean VS chop  
3. [x] Encode scales `height_scale?0.55·Hs`, `disp_scale?0.45·Hs·chop` (no GPU reduction readback)  
4. [x] Tests: `ocean_pass_test`, `rhi_test`, `ocean_system_test`; update spec honest limits  
