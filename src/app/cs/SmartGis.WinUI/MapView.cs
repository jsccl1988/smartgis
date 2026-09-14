// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
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
    public MapView()
    {
        Session = new MapSession();
        Background = new SolidColorBrush(Color.FromArgb(255, 28, 42, 58));
        HorizontalAlignment = HorizontalAlignment.Stretch;
        VerticalAlignment = VerticalAlignment.Stretch;
        SizeChanged += (_, _) => SyncIsland();
        Loaded += OnLoaded;
        Unloaded += (_, _) => Session.SetVisible(false);
    }

    public MapSession Session { get; }

    public IntPtr WindowHwnd { get; private set; }

    public void AttachWindow(IntPtr hwnd)
    {
        WindowHwnd = hwnd;
        Session.AttachParent(hwnd);
        Session.StartRender();
        Session.OpenView(0);
        SyncIsland();
    }

    public void ShowKind(int kind)
    {
        Session.ShowKind(kind);
        Session.SetVisible(true);
        SyncIsland();
    }

    public void SyncIsland()
    {
        if (WindowHwnd == IntPtr.Zero || ActualWidth < 1 || ActualHeight < 1)
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
        if (w < 1 || h < 1)
        {
            return;
        }
        Session.SyncLayout(x, y, w, h, (float)(scale * 96.0));
    }

    private void OnLoaded(object sender, RoutedEventArgs e)
    {
        SyncIsland();
    }
}
