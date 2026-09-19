// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

using System.Reflection;
using System.Runtime.InteropServices;

namespace SmartGis.Host;

/// <summary>P/Invoke resolver for smartgis_host[_d].dll beside the app.</summary>
internal static class Native
{
    static Native()
    {
        NativeLibrary.SetDllImportResolver(typeof(Native).Assembly, Resolve);
    }

    private static IntPtr Resolve(string libraryName, Assembly assembly, DllImportSearchPath? search)
    {
        if (libraryName != "smartgis_host")
        {
            return IntPtr.Zero;
        }
        var dir = AppContext.BaseDirectory;
        foreach (var name in new[] { "smartgis_host_d.dll", "smartgis_host.dll" })
        {
            var path = Path.Combine(dir, name);
            if (File.Exists(path) && NativeLibrary.TryLoad(path, out var handle))
            {
                return handle;
            }
        }
        return IntPtr.Zero;
    }

    [DllImport("smartgis_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr sg_host_create();

    [DllImport("smartgis_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern void sg_host_destroy(IntPtr host);

    [DllImport("smartgis_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int sg_host_start_render(IntPtr host);

    [DllImport("smartgis_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int sg_host_is_oop(IntPtr host);

    [DllImport("smartgis_host", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
    internal static extern IntPtr sg_host_present_status(IntPtr host);

    [DllImport("smartgis_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern uint sg_host_open_view(IntPtr host, int kind);

    [DllImport("smartgis_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern void sg_host_show_kind(IntPtr host, int kind);

    [DllImport("smartgis_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern uint sg_host_view_id(IntPtr host);

    [DllImport("smartgis_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int sg_host_view_kind(IntPtr host);

    [DllImport("smartgis_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern void sg_host_attach_parent(IntPtr host, IntPtr hwnd);

    [DllImport("smartgis_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern void sg_host_sync_layout(
        IntPtr host, int x, int y, int w, int h, float dpi);

    [DllImport("smartgis_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern void sg_host_set_visible(IntPtr host, int visible);

    [DllImport("smartgis_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr sg_host_map_child_hwnd(IntPtr host);

    [DllImport("smartgis_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int sg_host_has_synced_layout(IntPtr host);

    [DllImport("smartgis_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int sg_host_has_presented_frame(IntPtr host);

    [DllImport("smartgis_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int sg_host_has_live_pixels(IntPtr host);

    [DllImport("smartgis_host", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    internal static extern void sg_host_catalog_call(IntPtr host, string json);

    [DllImport("smartgis_host", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    internal static extern void sg_host_activate_tool(IntPtr host, string toolId);

    [DllImport("smartgis_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern void sg_host_dispatch_pointer(
        IntPtr host, int kind, int xPx, int yPx, int wheel);

    [DllImport("smartgis_host", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int sg_host_wait_frame(IntPtr host, uint timeoutMs);
}
