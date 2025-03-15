using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;

namespace Pebble_Companion;

public partial class App : Application {
    public override void Initialize() {
        AvaloniaXamlLoader.Load(this);
        RPC.Connect();
        
        // Use default port 5983 from PebbleWSServer
        int port = 5983;
        var server = new PebbleWSServer(port);
        _ = server.Start();
    }

    public override void OnFrameworkInitializationCompleted() {
        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop) {
            // Get local IP addresses
            var localIPs = GetLocalIPAddresses();
            int port = 5983; // Same port used when creating the server
            
            // Create main window with IP address info
            desktop.MainWindow = new MainWindow(localIPs, port);
        }

        base.OnFrameworkInitializationCompleted();
    }
    
    private List<string> GetLocalIPAddresses() {
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