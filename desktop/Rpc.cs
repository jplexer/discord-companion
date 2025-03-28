using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.WebSockets;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace Pebble_Companion;

public class Rpc {
    private static ClientWebSocket? _ws;
    private static string? _accessToken;
    private static JsonElement _voiceSettings;
    private static JsonElement _voiceChannel;
    private static string? _currentVoiceChannelId;
    private static int _voiceChannelUserCount;
    private static string? _currentServerName;
    private static bool _dmChannel;

    public static async Task Connect() {
        _ws = new ClientWebSocket();
        _ws.Options.SetRequestHeader("Origin", "http://localhost:3000");
        await _ws.ConnectAsync(new Uri("ws://127.0.0.1:6463/?v=1&encoding=json&client_id=207646673902501888"),
            CancellationToken.None);

        // Start receiving messages after connection
        _ = ReceiveMessagesAsync();
    }

    private static async Task GetAccessTokenStage1() {
        var payload = new {
            cmd = "AUTHORIZE",
            args = new {
                client_id = "207646673902501888",
                scopes = new[] { "rpc" },
                prompt = "none"
            },
            nonce = Guid.NewGuid().ToString("N"),
        };
        var buffer = Encoding.UTF8.GetBytes(JsonSerializer.Serialize(payload));
        await _ws?.SendAsync(new ArraySegment<byte>(buffer), WebSocketMessageType.Text, true, CancellationToken.None)!;
    }

    private static async Task GetAccessTokenStage2(string code) {
        const string url = "https://streamkit.discord.com/overlay/token";
        var payload = new { code };
        var request = new System.Net.Http.HttpRequestMessage(System.Net.Http.HttpMethod.Post, url);
        request.Content = new System.Net.Http.StringContent(JsonSerializer.Serialize(payload),
            Encoding.UTF8, "application/json");
        var response = await new System.Net.Http.HttpClient().SendAsync(request);
        var responseContent = await response.Content.ReadAsStringAsync();
        var responseJson = JsonDocument.Parse(responseContent).RootElement;
        _accessToken = responseJson.GetProperty("access_token").GetString();
        if (_accessToken == null) {
            Console.WriteLine("Failed to get access token");
            return;
        }

        await Authenticate();
    }

    private static async Task Authenticate() {
        var payload = new {
            cmd = "AUTHENTICATE",
            args = new {
                access_token = _accessToken,
            },
            nonce = Guid.NewGuid().ToString("N"),
        };
        var buffer = Encoding.UTF8.GetBytes(JsonSerializer.Serialize(payload));
        await _ws!.SendAsync(new ArraySegment<byte>(buffer), WebSocketMessageType.Text, true, CancellationToken.None);
    }

    private static async Task SubscribeToEvents() {
        await SendSubscription("VOICE_CHANNEL_SELECT");
        await SendSubscription("VOICE_SETTINGS_UPDATE");
    }

    public static async Task ToggleMute() {
        if (_voiceSettings.ValueKind == JsonValueKind.Undefined) {
            Console.WriteLine("Voice settings not available yet");
            return;
        }

        //set to opposite of current mute state
        await SendCommand("SET_VOICE_SETTINGS", new {
            mute = !_voiceSettings.GetProperty("mute").GetBoolean(),
        });
    }

    public static async Task ToggleDeafen() {
        if (_voiceSettings.ValueKind == JsonValueKind.Undefined) {
            Console.WriteLine("Voice settings not available yet");
            return;
        }

        //set to opposite of current mute state
        await SendCommand("SET_VOICE_SETTINGS", new {
            deaf = !_voiceSettings.GetProperty("deaf").GetBoolean(),
        });
    }

    public static async Task LeaveChannel() {
        await SendCommand("SELECT_VOICE_CHANNEL", new { channel_id = (string)null!, force = true });
    }


    public static async Task<JsonElement> GetInitialState() {
        if (_voiceSettings.ValueKind == JsonValueKind.Undefined) {
            Console.WriteLine("Voice settings not available yet");
            return new JsonElement();
        }

        // Get mute and deafen state from voice settings
        var muteSetting = _voiceSettings.GetProperty("mute").GetBoolean();
        var deafSetting = _voiceSettings.GetProperty("deaf").GetBoolean();

        // Default values for channel properties
        var channelName = "";
        object users = "";
        var serverName = "";

        // Update channel properties if voice channel is available
        if (_voiceChannel.ValueKind != JsonValueKind.Undefined) {
            channelName = _voiceChannel.GetProperty("name").GetString();
            users = _voiceChannelUserCount;
            serverName = _currentServerName;
        }

        // Create a single state object with all properties
        var state = new {
            cmd = "GET_INITIAL_STATE",
            mute = muteSetting,
            deaf = deafSetting,
            channelName,
            users,
            serverName
        };

        // Convert to JsonElement and return
        var jsonString = JsonSerializer.Serialize(state);
        return JsonDocument.Parse(jsonString).RootElement;
    }

    private static async Task SendSubscription(string evt, object? args = null) {
        var payload = new {
            cmd = "SUBSCRIBE",
            evt,
            args,
            nonce = Guid.NewGuid().ToString("N"),
        };

        var buffer = Encoding.UTF8.GetBytes(JsonSerializer.Serialize(payload));
        await _ws!.SendAsync(new ArraySegment<byte>(buffer), WebSocketMessageType.Text, true, CancellationToken.None);
    }

    private static async Task SendCommand(string cmd, object? args = null) {
        var payload = new {
            cmd,
            args,
            nonce = Guid.NewGuid().ToString("N"),
        };

        var buffer = Encoding.UTF8.GetBytes(JsonSerializer.Serialize(payload));
        await _ws!.SendAsync(new ArraySegment<byte>(buffer), WebSocketMessageType.Text, true, CancellationToken.None);
    }

    private static async Task SubscribeToVoiceStateEvents(string channelId) {
        await SendSubscription("VOICE_STATE_CREATE", new { channel_id = channelId });
        await SendSubscription("VOICE_STATE_DELETE", new { channel_id = channelId });
        Console.WriteLine($"Subscribed to voice state events for channel {channelId}");
    }

    private static async Task SendUnsubscription(string evt, object? args = null) {
        var payload = new {
            cmd = "UNSUBSCRIBE",
            evt,
            args,
            nonce = Guid.NewGuid().ToString("N"),
        };

        var buffer = Encoding.UTF8.GetBytes(JsonSerializer.Serialize(payload));
        await _ws!.SendAsync(new ArraySegment<byte>(buffer), WebSocketMessageType.Text, true, CancellationToken.None);
    }

    private static async Task UnsubscribeFromVoiceStateEvents(string? channelId) {
        await SendUnsubscription("VOICE_STATE_CREATE", new { channel_id = channelId });
        await SendUnsubscription("VOICE_STATE_DELETE", new { channel_id = channelId });
        Console.WriteLine($"Unsubscribed from voice state events for channel {channelId}");
    }

    private static async Task GetCurrentVoiceChannel() {
        await SendCommand("GET_SELECTED_VOICE_CHANNEL");
    }
    
    
    private static void LogError(string message, Exception? ex = null) {
        Console.WriteLine($"ERROR: {message}");
        if (ex == null) return;
        Console.WriteLine($"Exception: {ex.GetType().Name}");
        Console.WriteLine($"Message: {ex.Message}");
        Console.WriteLine($"Stack Trace: {ex.StackTrace}");
    }

    private static void LogMessage(string message) {
        Console.WriteLine($"INFO: {message}");
    }


    private static async Task ReceiveMessagesAsync() {
        var buffer = new byte[4096];
        try {
            while (_ws!.State == WebSocketState.Open) {
                var result = await _ws.ReceiveAsync(new ArraySegment<byte>(buffer), CancellationToken.None);

                switch (result.MessageType) {
                    case WebSocketMessageType.Text: {
                        var message = Encoding.UTF8.GetString(buffer, 0, result.Count);
                        // Trim any whitespace or invisible characters
                        message = message.Trim();

                        if (string.IsNullOrEmpty(message)) {
                            Console.WriteLine("Received empty message, skipping");
                            continue;
                        }

                        if (message.StartsWith("<")) {
                            Console.WriteLine(
                                $"Received HTML instead of JSON: {message.Substring(0, Math.Min(100, message.Length))}");
                            continue;
                        }

                        try {
                            var json = JsonDocument.Parse(message).RootElement;
                            if (json.TryGetProperty("cmd", out var cmdElement)) {
                                var cmd = cmdElement.GetString() ?? string.Empty;
                                json.TryGetProperty("evt", out var evtElement);
                                Console.WriteLine(message);
                                switch (cmd) {
                                    case "DISPATCH":
                                        // Handle dispatch events
                                        var eventType = evtElement.GetString();
                                        switch (eventType) {
                                            case "READY":
                                                await GetAccessTokenStage1();
                                                break;

                                            case "VOICE_STATE_CREATE":
                                                var voiceChannelFormerUserCount = _voiceChannelUserCount;
                                                _voiceChannelUserCount++;
                                                Console.WriteLine($"User joined voice channel. Total users: {_voiceChannelUserCount}");
                                                await PebbleWSServer.UserNumberChange(_voiceChannelUserCount);
                                                if (_dmChannel) {
                                                    if (_voiceChannelUserCount != 1 && voiceChannelFormerUserCount == 1) {
                                                        var userName =
                                                            json.GetProperty("data").GetProperty("nick").GetString() ??
                                                            string.Empty;
                                                        await PebbleWSServer.JoinedChannel(userName,
                                                            _voiceChannelUserCount);
                                                    }
                                                }
                                                break;

                                            case "VOICE_STATE_DELETE":
                                                if (_voiceChannelUserCount > 0) _voiceChannelUserCount--;
                                                Console.WriteLine(
                                                    $"User left voice channel. Total users: {_voiceChannelUserCount}");
                                                await PebbleWSServer.UserNumberChange(_voiceChannelUserCount);
                                                break;

                                            case "VOICE_CHANNEL_SELECT":
                                                var channelId = json.GetProperty("data").GetProperty("channel_id")
                                                    .GetString();
                                                if (json.GetProperty("data").TryGetProperty("guild_id", out var guildIdElement)) {
                                                    var guildId = guildIdElement.GetString();
                                                    LogMessage($"Found guild_id: {guildId ?? "null"}");
                                                } else {
                                                    LogMessage("No guild_id property found - might be a DM or group chat");
                                                }

                                                // Handle unsubscribing from previous channel if we were in one
                                                if (_currentVoiceChannelId != null &&
                                                    (channelId == null || channelId != _currentVoiceChannelId)) {
                                                    await UnsubscribeFromVoiceStateEvents(_currentVoiceChannelId);
                                                }

                                                if (channelId == null) {
                                                    _currentVoiceChannelId = null;
                                                    _voiceChannel = new JsonElement();
                                                    await PebbleWSServer.LeftChannel();
                                                }
                                                else {
                                                    // Subscribe to the new channel
                                                    _currentVoiceChannelId = channelId;
                                                    await SubscribeToVoiceStateEvents(channelId);
                                                    await SendCommand("GET_CHANNEL", new { channel_id = channelId });
                                                }

                                                break;

                                            case "VOICE_SETTINGS_UPDATE":
                                                _voiceSettings = json.GetProperty("data");
                                                await PebbleWSServer.UserVoiceStateUpdate(
                                                    _voiceSettings.GetProperty("mute").GetBoolean(),
                                                    _voiceSettings.GetProperty("deaf").GetBoolean());
                                                break;

                                            default:
                                                Console.WriteLine("Unhandled event: " + eventType);
                                                break;
                                        }

                                        break;
                                    case "GET_CHANNEL":
                                        _voiceChannel = json.GetProperty("data");
                                        if (_voiceChannel.TryGetProperty("voice_states", out var voiceStates)) {
                                            _voiceChannelUserCount = voiceStates.GetArrayLength();
                                            Console.WriteLine(
                                                $"Updated voice channel user count: {_voiceChannelUserCount}");
                                        }
                                        else {
                                            //voice channel user count is at least 1, because we are in the channel
                                            _voiceChannelUserCount = 1;
                                        }

                                        switch (_voiceChannel.GetProperty("type").GetInt16()) {
                                            case 0:
                                                Console.WriteLine("Text channel");
                                                break;
                                            case 2:
                                                _dmChannel = false;
                                                await SendCommand("GET_GUILD", new { guild_id = _voiceChannel.GetProperty("guild_id").GetString() });
                                                await PebbleWSServer.JoinedChannel(
                                                    "#" + _voiceChannel.GetProperty("name").GetString(),
                                                    _voiceChannelUserCount);
                                                break;
                                            case 1:
                                                _dmChannel = true;
                                                //if there is a user in the channel, we can get their name
                                                await PebbleWSServer.JoinedChannel(
                                                    _voiceChannel.GetProperty("voice_states").GetArrayLength() >= 1
                                                        ? _voiceChannel.GetProperty("voice_states")[0]
                                                            .GetProperty("nick").GetString()
                                                        : "Calling...", //Temporary String, because we can only get the other user once they join
                                                    _voiceChannelUserCount);


                                                await PebbleWSServer.ServerNameUpdate("Private Call");
                                                break;
                                            case 3:
                                                _dmChannel = false; //Even though it is technically a DM channel, we dont need special handling
                                                Console.WriteLine("Group DM channel");
                                                await PebbleWSServer.JoinedChannel(
                                                    _voiceChannel.GetProperty("name").GetString(),
                                                    _voiceChannelUserCount);
                                                await PebbleWSServer.ServerNameUpdate("Group Call");
                                                break;
                                            default:
                                                Console.WriteLine("Some new channel type?");
                                                break;
                                        }
                                        break;
                                    
                                    case "GET_GUILD":
                                        _currentServerName = json.GetProperty("data").GetProperty("name").GetString(); 
                                        await PebbleWSServer.ServerNameUpdate(_currentServerName);
                                        break;

                                    case "GET_SELECTED_VOICE_CHANNEL":
                                        if (json.TryGetProperty("data", out var channelData)) {
                                            // Check if data is null
                                            if (channelData.ValueKind == JsonValueKind.Null) {
                                                Console.WriteLine("User is not in a voice channel");
                                                _currentVoiceChannelId = null;
                                                _voiceChannelUserCount = 0;
                                                await PebbleWSServer.LeftChannel();
                                            }
                                            else if (channelData.TryGetProperty("id", out var idElement)) {
                                                var channelId = idElement.GetString();
                                                if (!string.IsNullOrEmpty(channelId)) {
                                                    _currentVoiceChannelId = channelId;
                                                    await SubscribeToVoiceStateEvents(channelId);
                                                    await SendCommand("GET_CHANNEL", new { channel_id = channelId });
                                                }
                                            }
                                        }

                                        break;

                                    case "AUTHORIZE":
                                        // Handle authorization response
                                        if (json.TryGetProperty("data", out var dataElement) &&
                                            dataElement.TryGetProperty("code", out var codeElement)) {
                                            var code = codeElement.GetString() ?? string.Empty;
                                            if (!string.IsNullOrEmpty(code)) {
                                                await GetAccessTokenStage2(code);
                                            }
                                        }

                                        break;

                                    case "AUTHENTICATE":
                                        if (evtElement.GetString() == "ERROR") {
                                            _accessToken = null;
                                            await GetAccessTokenStage1();
                                            Console.WriteLine("Authentication error, re-authenticating");
                                        }
                                        else {
                                            await SubscribeToEvents();
                                            await GetCurrentVoiceChannel();
                                        }

                                        break;
                                    default:
                                        Console.WriteLine("Unhandled command: " + cmd);
                                        break;
                                }
                            }
                        }
                        catch (JsonException ex) {
                            Console.WriteLine($"Invalid JSON received: {ex.Message}");

                            // Log byte-by-byte representation to identify any invisible characters
                            var bytes = Encoding.UTF8.GetBytes(message);
                            Console.WriteLine($"First 20 bytes: {BitConverter.ToString(bytes.Take(20).ToArray())}");
                            Console.WriteLine(
                                $"Message content: {message.Substring(0, Math.Min(200, message.Length))}");
                        }

                        break;
                    }
                    case WebSocketMessageType.Close:
                        await _ws.CloseAsync(WebSocketCloseStatus.NormalClosure, "Connection closed by server",
                            CancellationToken.None);
                        Console.WriteLine("WebSocket connection closed");
                        //Log reason by using result.CloseStatus and result.CloseStatusDescription
                        Console.WriteLine("Close status: " + result.CloseStatus);
                        Console.WriteLine("Close status description: " + result.CloseStatusDescription);

                        break;
                    case WebSocketMessageType.Binary:
                        Console.WriteLine("Received binary message, how did we get here?");
                        break;
                    default:
                        Console.WriteLine("Unknown message type");
                        break;
                }
            }
        } catch (KeyNotFoundException? ex) {
            LogError($"Dictionary key not found: {ex.Message}", ex);
        } catch (JsonException? ex) {
            LogError($"JSON parsing error: {ex.Message}", ex);
        } catch (Exception ex) {
            Console.WriteLine($"WebSocket error: {ex.Message}");

            // Try to reconnect after a short delay
            await Task.Delay(5000);
            try {
                if (_ws!.State != WebSocketState.Open) {
                    Console.WriteLine("Attempting to reconnect...");
                    await Connect();
                }
            }
            catch (Exception reconnectEx) {
                Console.WriteLine($"Reconnection failed: {reconnectEx.Message}");
            }
        }
    }
}