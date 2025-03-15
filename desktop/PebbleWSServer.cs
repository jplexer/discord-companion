using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.WebSockets;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace Pebble_Companion;

public class PebbleWSServer {
    // Singleton instance
    private static PebbleWSServer instance;

    // Lock object for thread safety
    private static readonly Lock lockObject = new Lock();

    // Singleton accessor
    public static PebbleWSServer Instance {
        get {
            if (instance == null) {
                lock (lockObject) {
                    instance ??= new PebbleWSServer();
                }
            }

            return instance;
        }
    }

    // Static method to notify about user number changes
    public static async Task UserNumberChange(int userNumber) {
        var message = JsonSerializer.Serialize(new { cmd = "USER_NUMBER_CHANGE", userNumber });
        await Instance.SendToAllAsync(message);
    }

    public static async Task LeftChannel() {
        var message = JsonSerializer.Serialize(new { cmd = "LEFT_CHANNEL" });
        await Instance.SendToAllAsync(message);
    }

    public static async Task JoinedChannel(string channelName, int userNumber) {
        var message = JsonSerializer.Serialize(new { cmd = "JOINED_CHANNEL", channelName, userNumber });
        await Instance.SendToAllAsync(message);
    }

    public static async Task UserVoiceStateUpdate(bool mute, bool deaf) {
        var message = JsonSerializer.Serialize(new { cmd = "USER_VOICE_STATE_UPDATE", mute, deaf });
        await Instance.SendToAllAsync(message);
    }

    // Rest of your existing code
    private HttpListener httpListener;
    private readonly string prefix;
    private readonly int port;
    private bool isRunning;
    private CancellationTokenSource cancellationTokenSource;
    private readonly List<WebSocket> connectedClients = new();

    public PebbleWSServer(int port = 5983, bool localOnly = false) {
        this.port = port;

        // Choose between localhost only or all interfaces
        this.prefix = localOnly ? $"http://localhost:{port}/" : $"http://*:{port}/"; // Listen on all interfaces

        LogMessage($"PebbleWSServer initialized with prefix: {prefix}");
    }

    public async Task Start() {
        if (isRunning) {
            LogMessage("Server already running, ignoring start request");
            return;
        }

        // IMPORTANT: Set the singleton instance to this instance
        lock (lockObject) {
            instance = this;
        }

        LogMessage($"Starting WebSocket server on {prefix}...");
        httpListener = new HttpListener();
        httpListener.Prefixes.Add(prefix);
        cancellationTokenSource = new CancellationTokenSource();

        try {
            httpListener.Start();
            isRunning = true;
            LogMessage("WebSocket server started and listening");

            // Start accepting connections
            _ = Task.Run(AcceptConnectionsLoopAsync, cancellationTokenSource.Token);
        }
        catch (Exception ex) {
            LogError($"Failed to start WebSocket server: {ex.Message}", ex);
            Stop();
        }
    }

    public void Stop() {
        if (!isRunning) {
            LogMessage("Server not running, ignoring stop request");
            return;
        }

        LogMessage("Stopping WebSocket server...");
        cancellationTokenSource?.Cancel();

        int clientCount = connectedClients.Count;
        LogMessage($"Closing {clientCount} client connection(s)");

        foreach (var client in connectedClients.ToArray()) {
            try {
                LogMessage($"Closing client connection with state: {client.State}");
                client.CloseAsync(WebSocketCloseStatus.NormalClosure,
                    "Server shutting down", CancellationToken.None).Wait(1000);
            }
            catch (Exception ex) {
                LogError("Error closing client connection", ex);
            }
        }

        connectedClients.Clear();

        try {
            LogMessage("Stopping HTTP listener");
            httpListener?.Stop();
            httpListener?.Close();
        }
        catch (Exception ex) {
            LogError("Error stopping HTTP listener", ex);
        }

        isRunning = false;
        LogMessage("WebSocket server stopped");
    }

    public async Task SendToAllAsync(string message) {
        if (string.IsNullOrEmpty(message)) {
            LogError("Cannot send null or empty message");
            return;
        }

        // Log first to diagnose if we're reaching this point
        LogMessage(
            $"Attempting to send message to {connectedClients.Count} clients: {message.Substring(0, Math.Min(50, message.Length))}{(message.Length > 50 ? "..." : "")}");

        if (connectedClients.Count == 0) {
            LogMessage("No clients connected, message will not be sent");
            return;
        }

        var buffer = Encoding.UTF8.GetBytes(message);
        var deadConnections = new List<WebSocket>();

        // Use ToList() to create a copy of the collection for thread safety
        foreach (var client in connectedClients.ToList()) {
            if (client == null) {
                deadConnections.Add(client);
                continue;
            }

            try {
                if (client.State == WebSocketState.Open) {
                    await client.SendAsync(
                        new ArraySegment<byte>(buffer),
                        WebSocketMessageType.Text,
                        true,
                        CancellationToken.None);

                    // Add success logging to confirm messages are sending
                    LogMessage($"Successfully sent message to client");
                }
                else {
                    LogMessage($"Client in non-open state: {client.State}, marking for removal");
                    deadConnections.Add(client);
                }
            }
            catch (Exception ex) {
                LogError($"Failed to send to client: {ex.Message}", ex);
                deadConnections.Add(client);
            }
        }

        // Clean up dead connections
        if (deadConnections.Count > 0) {
            LogMessage($"Removing {deadConnections.Count} dead connections");
            foreach (var deadClient in deadConnections.Where(c => c != null)) {
                connectedClients.Remove(deadClient);
            }

            LogMessage($"Remaining active connections: {connectedClients.Count}");
        }
    }

    private async Task AcceptConnectionsLoopAsync() {
        LogMessage("Started connection acceptance loop");
        while (isRunning && !cancellationTokenSource.Token.IsCancellationRequested) {
            try {
                LogMessage("Waiting for incoming connection...");
                var context = await httpListener.GetContextAsync();

                if (context.Request.IsWebSocketRequest) {
                    LogMessage("Processing WebSocket request");
                    _ = HandleWebSocketConnectionAsync(context);
                }
                else {
                    LogMessage("Received non-WebSocket HTTP request");
                    await using var writer = new System.IO.StreamWriter(context.Response.OutputStream);
                    context.Response.StatusCode = 200;
                    context.Response.ContentType = "text/plain";
                    await writer.WriteAsync(
                        "Pebble WebSocket server is running. Use a WebSocket connection to connect.");
                    context.Response.Close();
                    LogMessage("Responded to HTTP request");
                }
            }
            catch (HttpListenerException ex) {
                LogError($"HttpListener exception: {ex.Message}", ex);
                break;
            }
            catch (OperationCanceledException ex) {
                LogMessage("Operation was canceled: " + ex.Message);
                break;
            }
            catch (Exception ex) {
                LogError($"Error accepting connection: {ex.Message}", ex);
            }
        }

        LogMessage("Exiting connection acceptance loop");
    }

    private async Task HandleWebSocketConnectionAsync(HttpListenerContext context) {
        WebSocket webSocket = null;

        try {
            LogMessage($"Starting WebSocket handshake for client {context.Request.RemoteEndPoint}");
            WebSocketContext webSocketContext = await context.AcceptWebSocketAsync(subProtocol: null);
            webSocket = webSocketContext.WebSocket;

            connectedClients.Add(webSocket);
            LogMessage(
                $"WebSocket client connected successfully from {context.Request.RemoteEndPoint}, total clients: {connectedClients.Count}");

            await HandleClientMessagesAsync(webSocket, context.Request.RemoteEndPoint);
        }
        catch (Exception ex) {
            LogError($"WebSocket error from {context.Request.RemoteEndPoint}: {ex.Message}", ex);
            context.Response.StatusCode = 500;
            context.Response.Close();
        }
        finally {
            if (webSocket != null && connectedClients.Contains(webSocket)) {
                connectedClients.Remove(webSocket);
                LogMessage(
                    $"WebSocket client {context.Request.RemoteEndPoint} disconnected, remaining clients: {connectedClients.Count}");
            }
        }
    }

    private async Task HandleClientMessagesAsync(WebSocket webSocket, IPEndPoint clientEndpoint) {
        var buffer = new byte[4096];
        var receiveBuffer = new ArraySegment<byte>(buffer);

        try {
            LogMessage($"Starting message loop for client {clientEndpoint}");
            while (webSocket.State == WebSocketState.Open && !cancellationTokenSource.Token.IsCancellationRequested) {
                LogMessage($"Waiting for message from client {clientEndpoint}");
                WebSocketReceiveResult result =
                    await webSocket.ReceiveAsync(receiveBuffer, cancellationTokenSource.Token);

                if (result.MessageType == WebSocketMessageType.Text) {
                    string message = Encoding.UTF8.GetString(buffer, 0, result.Count);
                    LogMessage($"Received message from {clientEndpoint}: {message}");

                    try {
                        switch (message) {
                            case "mute":
                                LogMessage("Processing mute command");
                                await RPC.ToggleMute();
                                break;
                            case "deafen":
                                LogMessage("Processing deafen command");
                                await RPC.ToggleDeafen();
                                break;
                            case "getInitialState":
                                LogMessage("Processing getInitialState command");
                                var state = await RPC.GetInitialState();
                                // Convert to JSON string
                                var json = JsonSerializer.Serialize(state);
                                var bfr = Encoding.UTF8.GetBytes(json);
                                await webSocket.SendAsync(
                                    new ArraySegment<byte>(bfr),
                                    WebSocketMessageType.Text,
                                    true,
                                    CancellationToken.None);
                                break;
                            case "leaveChannel":
                                LogMessage("Processing leaveChannel command");
                                await RPC.LeaveChannel();
                                break;
                            default:
                                LogMessage($"Unknown command: {message}");
                                break;
                        }
                    }
                    catch (Exception ex) {
                        LogError($"Error processing command '{message}': {ex.Message}", ex);
                    }
                }
                else if (result.MessageType == WebSocketMessageType.Close) {
                    LogMessage($"Received close message from {clientEndpoint}");
                    await webSocket.CloseAsync(
                        WebSocketCloseStatus.NormalClosure,
                        "Closing",
                        CancellationToken.None);
                    break;
                }
                else {
                    LogMessage($"Received non-text message from {clientEndpoint}: {result.MessageType}");
                }
            }
        }
        catch (OperationCanceledException) {
            LogMessage($"WebSocket operation canceled for {clientEndpoint}");
        }
        catch (Exception ex) {
            LogError($"Error handling WebSocket messages from {clientEndpoint}: {ex.Message}", ex);
        }
        finally {
            try {
                if (webSocket.State != WebSocketState.Closed) {
                    LogMessage($"Closing WebSocket connection to {clientEndpoint}, current state: {webSocket.State}");
                    await webSocket.CloseAsync(
                        WebSocketCloseStatus.EndpointUnavailable,
                        "Server error",
                        CancellationToken.None);
                }
            }
            catch (Exception ex) {
                LogError($"Error closing WebSocket for {clientEndpoint}: {ex.Message}", ex);
            }
        }
    }

    private void LogMessage(string message) {
        string timestamp = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss.fff");
        Console.WriteLine($"[{timestamp}] [PebbleWS] {message}");
    }

    private void LogError(string message, Exception ex = null) {
        string timestamp = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss.fff");
        Console.WriteLine($"[{timestamp}] [PebbleWS] ERROR: {message}");
        if (ex != null) {
            Console.WriteLine($"[{timestamp}] [PebbleWS] Stack trace: {ex.StackTrace}");
        }
    }
}