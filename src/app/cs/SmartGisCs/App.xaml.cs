// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

using Microsoft.UI.Xaml;

namespace SmartGisCs;

public partial class App : Application
{
    private MainWindow? _window;

    public App()
    {
        InitializeComponent();
    }

    private static bool IsSelfTest()
    {
        return Environment.GetCommandLineArgs()
            .Any(a => string.Equals(a, "--self-test", StringComparison.Ordinal));
    }

    protected override async void OnLaunched(LaunchActivatedEventArgs args)
    {
        _window = new MainWindow();
        _window.Activate();
        if (!IsSelfTest())
        {
            return;
        }
        var code = await _window.RunSelfTestAsync();
        Environment.Exit(code);
    }
}
