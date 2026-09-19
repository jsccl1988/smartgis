// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

using Microsoft.UI.Dispatching;
using Microsoft.UI.Input;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using SmartGis.Host;
using Windows.Foundation;
using Windows.UI;

namespace SmartGis.WinUI;

/// <summary>
/// Reusable WinUI 3 map slot. Positions the native HWND island over this
/// control; present stays in smartgis_host (software DIB, same as MapHost).
/// </summary>
public sealed class MapView : Grid
{
    private int _attachStarted;
    private int _syncQueued;
    private int _syncDirty;

    public MapView()
    {
        Session = new MapSession();
        // Transparent so a mis-sized HWND overlay does not paint a false "empty map".
        Background = new SolidColorBrush(Color.FromArgb(0, 0, 0, 0));
        HorizontalAlignment = HorizontalAlignment.Stretch;
        VerticalAlignment = VerticalAlignment.Stretch;
        MinWidth = 120;
        MinHeight = 120;
        SizeChanged += (_, _) => QueueSyncIsland();
        Loaded += OnLoaded;
        Unloaded += (_, _) => Session.SetVisible(false);
        PointerPressed += OnPointerPressed;
        PointerMoved += OnPointerMoved;
        PointerReleased += OnPointerReleased;
        PointerWheelChanged += OnPointerWheelChanged;
    }

    public MapSession Session { get; }

    public IntPtr WindowHwnd { get; private set; }

    public bool IsRenderReady { get; private set; }

    public event Action<bool>? RenderReadyChanged;

    public void AttachWindow(IntPtr hwnd)
    {
        WindowHwnd = hwnd;
        Session.AttachParent(hwnd);
        // StartRenderProcess blocks up to ~30s on named-pipe + Hello. Doing that
        // on the UI thread freezes WinUI (white client area / Not Responding).
        if (Interlocked.Exchange(ref _attachStarted, 1) != 0)
        {
            QueueSyncIsland();
            return;
        }
        // Capture UI dispatcher before leaving the UI thread. OpenView /
        // SyncIsland / RenderReadyChanged must never run on the worker —
        // that yields XAML 0xc000027b (ERROR_INVALID_WINDOW_HANDLE).
        var queue = DispatcherQueue.GetForCurrentThread();
        _ = Task.Run(() =>
        {
            var ok = false;
            try
            {
                ok = Session.StartRender();
            }
            catch (Exception)
            {
                ok = false;
            }
            void Finish()
            {
                IsRenderReady = ok;
                if (ok)
                {
                    Session.OpenView(0);
                    QueueSyncIsland();
                }
                RenderReadyChanged?.Invoke(ok);
            }
            if (queue == null)
            {
                return;
            }
            for (var i = 0; i < 100; i++)
            {
                if (queue.TryEnqueue(Finish))
                {
                    return;
                }
                Thread.Sleep(20);
            }
        });
    }

    public void ShowKind(int kind)
    {
        Session.ShowKind(kind);
        Session.SetVisible(true);
        QueueSyncIsland();
    }

    public void SyncIsland() => QueueSyncIsland();

    // SetWindowPos during a XAML click/layout pass yields combase 0x80070578
    // (ERROR_INVALID_WINDOW_HANDLE) stowed as 0xc000027b. Always hop to the
    // next dispatcher tick so chrome clicks don't nest native HWND moves.
    // Coalesce carefully: a SizeChanged while a sync is queued must not drop
    // the newer bounds (otherwise the popup sticks at a tiny first layout).
    private void QueueSyncIsland()
    {
        Interlocked.Exchange(ref _syncDirty, 1);
        var queue = DispatcherQueue;
        if (queue == null)
        {
            Interlocked.Exchange(ref _syncDirty, 0);
            ApplySyncIsland();
            return;
        }
        if (Interlocked.Exchange(ref _syncQueued, 1) != 0)
        {
            return;
        }
        if (!queue.TryEnqueue(() =>
            {
                Interlocked.Exchange(ref _syncQueued, 0);
                while (Interlocked.Exchange(ref _syncDirty, 0) != 0)
                {
                    ApplySyncIsland();
                }
            }))
        {
            Interlocked.Exchange(ref _syncQueued, 0);
            Interlocked.Exchange(ref _syncDirty, 0);
            ApplySyncIsland();
        }
    }

    private void ApplySyncIsland()
    {
        if (WindowHwnd == IntPtr.Zero || ActualWidth < 8 || ActualHeight < 8)
        {
            return;
        }
        var scale = XamlRoot?.RasterizationScale ?? 1.0;
        Point origin;
        try
        {
            UIElement? relative = null;
            if (XamlRoot?.Content is UIElement content)
            {
                relative = content;
            }
            var transform = relative != null
                ? TransformToVisual(relative)
                : TransformToVisual(null);
            origin = transform.TransformPoint(new Point(0, 0));
        }
        catch (Exception)
        {
            return;
        }
        var x = (int)Math.Round(origin.X * scale);
        var y = (int)Math.Round(origin.Y * scale);
        var w = (int)Math.Round(ActualWidth * scale);
        var h = (int)Math.Round(ActualHeight * scale);
        if (w < 8 || h < 8)
        {
            return;
        }
        try
        {
            Session.SyncLayout(x, y, w, h, (float)(scale * 96.0));
        }
        catch (Exception)
        {
        }
    }

    private void OnLoaded(object sender, RoutedEventArgs e)
    {
        QueueSyncIsland();
    }

    private void DispatchXamlPointer(int kind, PointerRoutedEventArgs e, int wheel = 0)
    {
        try
        {
            var pt = e.GetCurrentPoint(this);
            var scale = XamlRoot?.RasterizationScale ?? 1.0;
            var x = (int)Math.Round(pt.Position.X * scale);
            var y = (int)Math.Round(pt.Position.Y * scale);
            Session.DispatchPointer(kind, x, y, wheel);
        }
        catch (Exception)
        {
        }
    }

    private void OnPointerPressed(object sender, PointerRoutedEventArgs e)
    {
        var right = e.GetCurrentPoint(this).Properties.IsRightButtonPressed;
        DispatchXamlPointer(right ? 5 : 2, e);
    }

    private void OnPointerMoved(object sender, PointerRoutedEventArgs e)
    {
        DispatchXamlPointer(0, e);
    }

    private void OnPointerReleased(object sender, PointerRoutedEventArgs e)
    {
        var right = e.GetCurrentPoint(this).Properties.IsRightButtonPressed;
        DispatchXamlPointer(right ? 6 : 3, e);
    }

    // Wheel/pan: native sg_host StretchBlts the last DIB; full MapScene paint
    // is debounced (~200ms) so 1338-feature China does not tessellate every tick.
    private void OnPointerWheelChanged(object sender, PointerRoutedEventArgs e)
    {
        DispatchXamlPointer(1, e, e.GetCurrentPoint(this).Properties.MouseWheelDelta);
    }
}
