using Avalonia.Controls;
using System.Collections.Generic;
using System.Linq;

namespace Pebble_Companion;

public partial class MainWindow : Window {
    public MainWindow()
    {
        InitializeComponent();
        this.Closing += MainWindow_Closing;
    }
    private void MainWindow_Closing(object sender, WindowClosingEventArgs e)
    {
        // Cancel the close operation
        e.Cancel = true;
        
        // Hide the window instead
        this.Hide();
    }
    
    public MainWindow(List<string> localIPs, int port) : this() {
        // After initialization, add IP information to the window
        ShowIPAddressInfo(localIPs, port);
    }

    private void ShowIPAddressInfo(List<string> localIPs, int port) {
        // Create a textblock to display the information
        var textBlock = new TextBlock {
            Text = GetIPAddressDisplayText(localIPs, port),
            Margin = new Avalonia.Thickness(10),
            TextWrapping = Avalonia.Media.TextWrapping.Wrap
        };

        // Add to the main content area
        if (Content is Panel panel) {
            panel.Children.Add(textBlock);
        }
        else {
            // If content is not already a panel, create one
            var stackPanel = new StackPanel();
            if (Content != null) {
                // Handle the case when Content is a string
                if (Content is string stringContent) {
                    stackPanel.Children.Add(new TextBlock { Text = stringContent });
                }
                // Handle the case when Content is a Control
                else if (Content is Control control) {
                    stackPanel.Children.Add(control);
                }
            }

            stackPanel.Children.Add(textBlock);
            Content = stackPanel;
        }
    }

    private string GetIPAddressDisplayText(List<string> localIPs, int port) {
        if (!localIPs.Any()) {
            return $"Pebble WebSocket server running on port {port}\nNo network interfaces found.";
        }
        
        var text = $"Pebble WebSocket server running on port {port}\n\n" +
                   "Configure your Pebble app using one of these addresses:\n";
        
        foreach (var ip in localIPs) {
            text += $"• {ip}\n";
        }
        
        text += "\nNote: You can close this window, the server will continue running in the background.";
        
        return text;
    }
}