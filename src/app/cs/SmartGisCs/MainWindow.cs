// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

using System.Text;
using Microsoft.UI;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using SmartGis.WinUI;
using Windows.Graphics;
using Windows.Storage.Pickers;
using Windows.UI;
using WinRT.Interop;

namespace SmartGisCs;

/// <summary>
/// Product chrome aligned with Views / WinUI C++: MenuBar, Catalog, Map|Data|3D,
/// Ambox, Inspector, StatusBar. Command ids match Workspace builtins.
/// </summary>
public sealed class MainWindow : Window
{
    private readonly MapView _map = new();
    private readonly MenuBar _menu = new();
    private readonly Grid _catalog = new();
    private readonly TreeView _catalogTree = new();
    private readonly StackPanel _ambox = new();
    private readonly Grid _mapColumn = new();
    private readonly Button _tabMap = MakeTab("Map Edit");
    private readonly Button _tabData = MakeTab("Data");
    private readonly Button _tabScene = MakeTab("3D");
    private readonly Grid _inspector = new();
    private readonly Button _inspFeature = MakeTab("FeatureInfo");
    private readonly Button _inspAttrs = MakeTab("AttributeTable");
    private readonly TextBlock _inspectorBody = new();
    private readonly TextBlock _status = new();
    private int _activeTab;

    public MainWindow()
    {
        Title = "SmartGIS WinUI (C#)";
        try
        {
            AppWindow.Resize(new SizeInt32(1280, 800));
        }
        catch (Exception)
        {
        }
        Content = BuildChrome();
        Activated += OnActivated;
    }

    public MapView Map => _map;

    public bool HasIdeChrome =>
        _menu.Items.Count > 0 && _catalogTree != null && _ambox.Children.Count > 0 &&
        _tabMap != null && _tabData != null && _tabScene != null &&
        _inspFeature != null && _inspAttrs != null && _status != null &&
        _map != null;

    public IntPtr NativeHwnd => WindowNative.GetWindowHandle(this);

    public void SelectMapTab(int index)
    {
        if (index < 0 || index > 2)
        {
            return;
        }
        _activeTab = index;
        HighlightMapTab(index);
        _map.SetVisible(true);
        _map.ShowKind(index);
        if (index == 2)
        {
            RunTool("view3d.trackball");
            SetStatus("3D");
        }
        else
        {
            RunTool("view.pan");
            SetStatus(index == 0 ? "Map Edit" : "Data");
        }
    }

    public bool RunTool(string commandId)
    {
        var id = commandId switch
        {
            "select" or "identify" => "selection.point",
            "pan" => "view.pan",
            _ => commandId,
        };
        if (_map.Session.ViewId == 0)
        {
            return false;
        }
        _map.Session.ActivateTool(id);
        SetStatus(id switch
        {
            "selection.clear" => "Selection cleared",
            "edit.append.point" => "Committed",
            _ => "Activated " + id,
        });
        return true;
    }

    public void SetStatus(string text) => _status.Text = text;

    public async Task<int> RunSelfTestAsync()
    {
        var markPath = Path.Combine(AppContext.BaseDirectory, "self-test-mark.txt");
        try
        {
            File.Delete(markPath);
        }
        catch (Exception)
        {
        }
        void Mark(string step)
        {
            try
            {
                File.AppendAllText(markPath, step + Environment.NewLine);
            }
            catch (Exception)
            {
            }
        }
        Mark("show");
        if (NativeHwnd == IntPtr.Zero)
        {
            Mark("exit-2");
            return 2;
        }
        if (!HasIdeChrome)
        {
            Mark("exit-4");
            return 4;
        }
        Mark("hwnd-ok");
        Mark("catalog-ok");
        var layoutOk = false;
        for (var i = 0; i < 60; i++)
        {
            _map.SyncIsland();
            await Task.Delay(33);
            if (_map.Session.HasSyncedLayout)
            {
                layoutOk = true;
                break;
            }
        }
        if (!layoutOk)
        {
            Mark("layout-fail");
        }
        if (_map.Session.MapChildHwnd == IntPtr.Zero)
        {
            Mark("no-child-hwnd");
            Mark("exit-4");
            return 4;
        }
        if (!_map.Session.HasSyncedLayout)
        {
            Mark("layout-small");
            Mark("exit-6");
            return 6;
        }
        if (!_map.Session.IsOop)
        {
            Mark("oop-fail");
            Mark("exit-7");
            return 7;
        }
        if (!_map.Session.WaitFrame(12000))
        {
            Mark("map-frame-fail");
            Mark("exit-3");
            return 3;
        }
        Mark("map-ready");
        Mark("map-frame-ok");

        SelectMapTab(1);
        await Pump(16);
        if (!_map.Session.WaitFrame(12000))
        {
            Mark("data-frame-fail");
            Mark("exit-8");
            return 8;
        }
        Mark("data-ready");

        SelectMapTab(2);
        await Pump(20);
        if (!_map.Session.WaitFrame(12000))
        {
            Mark("scene-frame-fail");
            Mark("exit-10");
            return 10;
        }
        Mark("scene-ready");
        Mark("scene-frame-ok");

        SelectMapTab(0);
        await Pump(8);
        RunTool("selection.point");
        RunTool("selection.clear");
        Mark("selection-ok");
        Mark("pass");
        Mark("exit-0");
        return 0;
    }

    private async Task Pump(int ticks)
    {
        for (var i = 0; i < ticks; i++)
        {
            _map.SyncIsland();
            await Task.Delay(16);
        }
    }

    private void OnActivated(object sender, WindowActivatedEventArgs args)
    {
        if (args.WindowActivationState == WindowActivationState.Deactivated)
        {
            return;
        }
        if (_map.WindowHwnd == IntPtr.Zero)
        {
            _map.AttachWindow(NativeHwnd);
            DispatcherQueue.TryEnqueue(() => _map.SyncIsland());
        }
    }

    private UIElement BuildChrome()
    {
        var root = new Grid { Background = new SolidColorBrush(Colors.Black) };
        root.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        root.RowDefinitions.Add(new RowDefinition { Height = new GridLength(1, GridUnitType.Star) });
        root.RowDefinitions.Add(new RowDefinition { Height = new GridLength(160) });
        root.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });

        WireMenu();
        Grid.SetRow(_menu, 0);
        root.Children.Add(_menu);

        var work = new Grid();
        work.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(240) });
        work.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        work.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(200) });

        WireCatalog();
        Grid.SetColumn(_catalog, 0);
        work.Children.Add(_catalog);

        WireMapTabs();
        Grid.SetColumn(_mapColumn, 1);
        work.Children.Add(_mapColumn);

        WireAmbox();
        Grid.SetColumn(_ambox, 2);
        work.Children.Add(_ambox);

        Grid.SetRow(work, 1);
        root.Children.Add(work);

        WireInspector();
        Grid.SetRow(_inspector, 2);
        root.Children.Add(_inspector);

        _status.Text = "Ready";
        _status.Margin = new Thickness(8, 4, 8, 4);
        _status.Foreground = new SolidColorBrush(Colors.LightGray);
        Grid.SetRow(_status, 3);
        root.Children.Add(_status);

        root.SizeChanged += (_, _) => _map.SyncIsland();
        return root;
    }

    private void WireMenu()
    {
        var file = new MenuBarItem { Title = "File" };
        file.Items.Add(Flyout("Open", () => _ = OnOpen()));
        file.Items.Add(Flyout("Exit", () => Application.Current.Exit()));
        _menu.Items.Add(file);

        var view = new MenuBarItem { Title = "View" };
        view.Items.Add(Flyout("Map Edit", () => SelectMapTab(0)));
        view.Items.Add(Flyout("Data", () => SelectMapTab(1)));
        view.Items.Add(Flyout("3D", () => SelectMapTab(2)));
        _menu.Items.Add(view);

        var tools = new MenuBarItem { Title = "Tools" };
        tools.Items.Add(Flyout("Select", () => RunTool("selection.point")));
        tools.Items.Add(Flyout("Draw", () => RunTool("edit.append.point")));
        tools.Items.Add(Flyout("Clear", () => RunTool("selection.clear")));
        tools.Items.Add(Flyout("Pan", () => RunTool("view.pan")));
        _menu.Items.Add(tools);
    }

    private void WireCatalog()
    {
        _catalog.Background = new SolidColorBrush(Colors.DimGray);
        _catalog.Width = 240;
        _catalog.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        _catalog.RowDefinitions.Add(new RowDefinition { Height = new GridLength(1, GridUnitType.Star) });
        _catalog.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });

        var title = new TextBlock
        {
            Text = "Catalog",
            Margin = new Thickness(8, 8, 8, 4),
            Foreground = new SolidColorBrush(Colors.White),
        };
        Grid.SetRow(title, 0);
        _catalog.Children.Add(title);

        var root = new TreeViewNode { Content = "Workspace" };
        root.Children.Add(new TreeViewNode { Content = "Layers" });
        root.Children.Add(new TreeViewNode { Content = "Maps" });
        _catalogTree.RootNodes.Add(root);
        Grid.SetRow(_catalogTree, 1);
        _catalog.Children.Add(_catalogTree);

        var cmds = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Margin = new Thickness(4),
        };
        var refresh = new Button { Content = "Refresh" };
        refresh.Click += (_, _) =>
        {
            _map.Session.CatalogCall("{\"op\":\"refresh\"}");
            SetStatus("Catalog refresh");
        };
        var add = new Button { Content = "Add layer" };
        add.Click += (_, _) => _ = OnOpen();
        cmds.Children.Add(refresh);
        cmds.Children.Add(add);
        Grid.SetRow(cmds, 2);
        _catalog.Children.Add(cmds);
    }

    private void WireAmbox()
    {
        _ambox.Width = 200;
        _ambox.Background = new SolidColorBrush(Colors.DimGray);
        _ambox.Padding = new Thickness(8);
        _ambox.Children.Add(new TextBlock
        {
            Text = "Ambox",
            Foreground = new SolidColorBrush(Colors.White),
            Margin = new Thickness(0, 0, 0, 8),
        });
        foreach (var id in new[]
                 {
                     "selection.point", "selection.clear", "edit.append.point",
                     "view.pan", "view.zoom_in", "view.zoom_out", "view3d.trackball",
                 })
        {
            var btn = new Button
            {
                Content = id,
                HorizontalAlignment = HorizontalAlignment.Stretch,
                Margin = new Thickness(0, 2, 0, 2),
            };
            var captured = id;
            btn.Click += (_, _) => RunTool(captured);
            _ambox.Children.Add(btn);
        }
    }

    private void WireInspector()
    {
        _inspector.Background = new SolidColorBrush(Colors.DarkSlateGray);
        _inspector.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        _inspector.RowDefinitions.Add(new RowDefinition { Height = new GridLength(1, GridUnitType.Star) });
        var tabs = new StackPanel { Orientation = Orientation.Horizontal };
        _inspFeature.Click += (_, _) =>
        {
            SetTabActive(_inspFeature, true);
            SetTabActive(_inspAttrs, false);
            _inspectorBody.Text = "FeatureInfo";
        };
        _inspAttrs.Click += (_, _) =>
        {
            SetTabActive(_inspFeature, false);
            SetTabActive(_inspAttrs, true);
            _inspectorBody.Text = "AttributeTable";
        };
        tabs.Children.Add(_inspFeature);
        tabs.Children.Add(_inspAttrs);
        Grid.SetRow(tabs, 0);
        _inspector.Children.Add(tabs);
        _inspectorBody.Text = "FeatureInfo";
        _inspectorBody.Margin = new Thickness(12, 8, 12, 8);
        _inspectorBody.Foreground = new SolidColorBrush(Colors.White);
        Grid.SetRow(_inspectorBody, 1);
        _inspector.Children.Add(_inspectorBody);
        SetTabActive(_inspFeature, true);
        SetTabActive(_inspAttrs, false);
    }

    private void WireMapTabs()
    {
        _mapColumn.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        _mapColumn.RowDefinitions.Add(new RowDefinition { Height = new GridLength(1, GridUnitType.Star) });
        var tabs = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Background = new SolidColorBrush(Colors.Black),
        };
        _tabMap.Click += (_, _) => SelectMapTab(0);
        _tabData.Click += (_, _) => SelectMapTab(1);
        _tabScene.Click += (_, _) => SelectMapTab(2);
        tabs.Children.Add(_tabMap);
        tabs.Children.Add(_tabData);
        tabs.Children.Add(_tabScene);
        Grid.SetRow(tabs, 0);
        _mapColumn.Children.Add(tabs);
        Grid.SetRow(_map, 1);
        _mapColumn.Children.Add(_map);
        HighlightMapTab(0);
    }

    private async Task OnOpen()
    {
        var picker = new FileOpenPicker();
        InitializeWithWindow.Initialize(picker, NativeHwnd);
        picker.FileTypeFilter.Add("*");
        var file = await picker.PickSingleFileAsync();
        if (file == null)
        {
            SetStatus("Open dialog unavailable");
            return;
        }
        var path = file.Path;
        var json = "{\"op\":\"open\",\"path\":\"" + JsonEscape(path) + "\"}";
        _map.Session.CatalogCall(json);
        if (_catalogTree.RootNodes.Count > 0)
        {
            var root = _catalogTree.RootNodes[0];
            TreeViewNode? maps = null;
            foreach (var child in root.Children)
            {
                if (child.Content is string s && s == "Maps")
                {
                    maps = child;
                    break;
                }
            }
            maps?.Children.Add(new TreeViewNode { Content = path });
        }
        SetStatus("Opened");
    }

    private void HighlightMapTab(int index)
    {
        SetTabActive(_tabMap, index == 0);
        SetTabActive(_tabData, index == 1);
        SetTabActive(_tabScene, index == 2);
    }

    private static Button MakeTab(string label)
    {
        return new Button
        {
            Content = label,
            Margin = new Thickness(2),
            Padding = new Thickness(10, 4, 10, 4),
        };
    }

    private static void SetTabActive(Button btn, bool active)
    {
        btn.Background = new SolidColorBrush(active ? Colors.SteelBlue : Colors.Transparent);
        btn.Foreground = new SolidColorBrush(Colors.White);
    }

    private static MenuFlyoutItem Flyout(string text, Action click)
    {
        var item = new MenuFlyoutItem { Text = text };
        item.Click += (_, _) => click();
        return item;
    }

    private static string JsonEscape(string text)
    {
        var sb = new StringBuilder(text.Length);
        foreach (var c in text)
        {
            if (c is '\\' or '"')
            {
                sb.Append('\\');
            }
            sb.Append(c);
        }
        return sb.ToString();
    }
}

internal static class MapViewVisible
{
    public static void SetVisible(this MapView map, bool visible) =>
        map.Session.SetVisible(visible);
}
