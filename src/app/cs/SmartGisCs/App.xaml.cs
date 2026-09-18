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
        UnhandledException += OnUnhandledException;
    }

    private static void OnUnhandledException(object sender, Microsoft.UI.Xaml.UnhandledExceptionEventArgs e)
    {
        try
        {
            var path = Path.Combine(AppContext.BaseDirectory, "smartgiscs-unhandled.log");
            File.AppendAllText(path, DateTime.Now.ToString("o") + " " + e.Exception + Environment.NewLine);
        }
        catch (Exception)
        {
        }
        // Last-resort: island HWND already gone. Root fix is top-level parenting
        // + deferred SetWindowPos; this keeps chrome clicks from taking the PE down.
        if (unchecked((uint)e.Exception.HResult) == 0x80070578u)
        {
            e.Handled = true;
        }
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
