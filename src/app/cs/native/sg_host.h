// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_CS_NATIVE_SG_HOST_H_
#define APP_CS_NATIVE_SG_HOST_H_

#include <stdint.h>

#if defined(_WIN32)
#if defined(SG_HOST_IMPLEMENTATION)
#define SG_HOST_EXPORT __declspec(dllexport)
#else
#define SG_HOST_EXPORT __declspec(dllimport)
#endif
#else
#define SG_HOST_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Language-neutral chrome FFI over content::MapContents + HWND island.
// C# P/Invoke and L0 tests call these; GIS objects stay in the GPU process.

typedef struct SgHost SgHost;

SG_HOST_EXPORT SgHost* sg_host_create(void);
SG_HOST_EXPORT void sg_host_destroy(SgHost* host);

SG_HOST_EXPORT int sg_host_start_render(SgHost* host);
SG_HOST_EXPORT int sg_host_is_oop(const SgHost* host);
SG_HOST_EXPORT const wchar_t* sg_host_present_status(const SgHost* host);

// kind: 0 = Map Edit, 1 = Data, 2 = 3D. Views stay open across show_kind.
SG_HOST_EXPORT uint32_t sg_host_open_view(SgHost* host, int kind);
SG_HOST_EXPORT void sg_host_show_kind(SgHost* host, int kind);
SG_HOST_EXPORT uint32_t sg_host_view_id(const SgHost* host);
SG_HOST_EXPORT int sg_host_view_kind(const SgHost* host);

SG_HOST_EXPORT void sg_host_attach_parent(SgHost* host, void* hwnd);
SG_HOST_EXPORT void sg_host_sync_layout(SgHost* host,
                                        int x,
                                        int y,
                                        int w,
                                        int h,
                                        float dpi);
SG_HOST_EXPORT void sg_host_set_visible(SgHost* host, int visible);
SG_HOST_EXPORT void* sg_host_map_child_hwnd(const SgHost* host);
SG_HOST_EXPORT int sg_host_has_synced_layout(const SgHost* host);
SG_HOST_EXPORT int sg_host_has_presented_frame(const SgHost* host);
SG_HOST_EXPORT int sg_host_has_live_pixels(const SgHost* host);

SG_HOST_EXPORT void sg_host_catalog_call(SgHost* host, const char* json);
SG_HOST_EXPORT void sg_host_activate_tool(SgHost* host, const char* tool_id);
// kind matches content::InputEvent::Kind (0=move, 1=wheel, 2=ldown, 3=lup).
SG_HOST_EXPORT void sg_host_dispatch_pointer(SgHost* host,
                                             int kind,
                                             int x_px,
                                             int y_px,
                                             int wheel);
SG_HOST_EXPORT int sg_host_wait_frame(SgHost* host, uint32_t timeout_ms);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // APP_CS_NATIVE_SG_HOST_H_
