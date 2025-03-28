using System;
using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using Avalonia.Platform;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using Avalonia.Controls;
using Avalonia.Data.Core;

namespace Pebble_Companion;

public partial class App : Application {    
    private MainWindow? _mainWindow;
    private NativeMenuItem? _showMenuItem;
    private NativeMenuItem? _exitMenuItem;
    public override void Initialize() {
        AvaloniaXamlLoader.Load(this);
        Rpc.Connect();
        
        // Use default port 5983 from PebbleWSServer
        int port = 5983;
        var server = new PebbleWSServer(port);
        _ = server.Start();
    }

    public override void OnFrameworkInitializationCompleted() {
        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop) {
            // Create main window
            _mainWindow = new MainWindow(GetLocalIPAddresses(), 5983);
            desktop.MainWindow = _mainWindow;

            // Set up tray icon menu with ViewModel
            var trayIcon = (TrayIcon)this.Resources["TrayIcon"];
            if (trayIcon is { Menu: NativeMenu menu })
            {

                // Store references to menu items
                _showMenuItem = menu.Items.ElementAtOrDefault(0) as NativeMenuItem;
                _exitMenuItem = menu.Items.ElementAtOrDefault(1) as NativeMenuItem;
    
                // Wire up direct method references instead of lambda expressions
                if (_showMenuItem != null)
                    _showMenuItem.Click += ShowMenuItem_Click;
    
                if (_exitMenuItem != null)
                    _exitMenuItem.Click += ExitMenuItem_Click;
            }

            base.OnFrameworkInitializationCompleted();
        }
    }
    
    private void ShowMenuItem_Click(object? sender, EventArgs e)
    {
        if (_mainWindow != null)
        {
            _mainWindow.Show();
            _mainWindow.WindowState = WindowState.Normal;
            _mainWindow.Activate();
        }
    }

    private void ExitMenuItem_Click(object? sender, EventArgs e)
    {
        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
        {
            desktop.Shutdown();
        }
    }

    private static List<string> GetLocalIPAddresses() {
        var addresses = new List<string>();
        
        // Get all network interfaces
        var networkInterfaces = NetworkInterface.GetAllNetworkInterfaces()
            .Where(ni => ni.OperationalStatus == OperationalStatus.Up && 
                  (ni.NetworkInterfaceType == NetworkInterfaceType.Wireless80211 || 
                   ni.NetworkInterfaceType == NetworkInterfaceType.Ethernet));
                   
        foreach (var networkInterface in networkInterfaces) {
            var properties = networkInterface.GetIPProperties();
            
            // Get IPv4 addresses
            foreach (var address in properties.UnicastAddresses) {
                if (address.Address.AddressFamily == AddressFamily.InterNetwork) {
                    addresses.Add(address.Address.ToString());
                }
            }
        }
        
        return addresses;
    }
}