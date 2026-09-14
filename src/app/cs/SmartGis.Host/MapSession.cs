// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

using System.Runtime.InteropServices;

namespace SmartGis.Host;

/// <summary>
/// Chrome-side map session over the native C ABI. GIS objects stay in the GPU
/// process; this type must not LoadLibrary SmtGisCore.
/// </summary>
public sealed class MapSession : IDisposable
{
    private IntPtr _handle;

    public MapSession()
    {
        _handle = Native.sg_host_create();
        if (_handle == IntPtr.Zero)
        {
            throw new InvalidOperationException("sg_host_create failed");
        }
    }

    public IntPtr Handle => _handle;

    public bool StartRender()
    {
        ThrowIfDisposed();
        return Native.sg_host_start_render(_handle) != 0;
    }

    public bool IsOop => _handle != IntPtr.Zero && Native.sg_host_is_oop(_handle) != 0;

    public string PresentStatus
    {
        get
        {
            ThrowIfDisposed();
            var p = Native.sg_host_present_status(_handle);
            return p == IntPtr.Zero ? "down" : Marshal.PtrToStringUni(p) ?? "down";
        }
    }

    public uint OpenView(int kind)
    {
        ThrowIfDisposed();
        return Native.sg_host_open_view(_handle, kind);
    }

    public void ShowKind(int kind)
    {
        ThrowIfDisposed();
        Native.sg_host_show_kind(_handle, kind);
    }

    public uint ViewId
    {
        get
        {
            ThrowIfDisposed();
            return Native.sg_host_view_id(_handle);
        }
    }

    public int ViewKind
    {
        get
        {
            ThrowIfDisposed();
            return Native.sg_host_view_kind(_handle);
        }
    }

    public void AttachParent(IntPtr hwnd)
    {
        ThrowIfDisposed();
        Native.sg_host_attach_parent(_handle, hwnd);
    }

    public void SyncLayout(int x, int y, int w, int h, float dpi)
    {
        ThrowIfDisposed();
        Native.sg_host_sync_layout(_handle, x, y, w, h, dpi);
    }

    public void SetVisible(bool visible)
    {
        ThrowIfDisposed();
        Native.sg_host_set_visible(_handle, visible ? 1 : 0);
    }

    public IntPtr MapChildHwnd
    {
        get
        {
            ThrowIfDisposed();
            return Native.sg_host_map_child_hwnd(_handle);
        }
    }

    public bool HasSyncedLayout =>
        _handle != IntPtr.Zero && Native.sg_host_has_synced_layout(_handle) != 0;

    public bool HasPresentedFrame =>
        _handle != IntPtr.Zero && Native.sg_host_has_presented_frame(_handle) != 0;

    public bool HasLivePixels =>
        _handle != IntPtr.Zero && Native.sg_host_has_live_pixels(_handle) != 0;

    public void CatalogCall(string json)
    {
        ThrowIfDisposed();
        Native.sg_host_catalog_call(_handle, json);
    }

    public void ActivateTool(string toolId)
    {
        ThrowIfDisposed();
        Native.sg_host_activate_tool(_handle, toolId);
    }

    public bool WaitFrame(uint timeoutMs)
    {
        ThrowIfDisposed();
        return Native.sg_host_wait_frame(_handle, timeoutMs) != 0;
    }

    public void Dispose()
    {
        if (_handle != IntPtr.Zero)
        {
            Native.sg_host_destroy(_handle);
            _handle = IntPtr.Zero;
        }
        GC.SuppressFinalize(this);
    }

    ~MapSession()
    {
        Dispose();
    }

    private void ThrowIfDisposed()
    {
        ObjectDisposedException.ThrowIf(_handle == IntPtr.Zero, this);
    }
}
