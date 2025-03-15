using System;
using System.Linq;
using System.Net.WebSockets;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace Pebble_Companion;

public class RPC {
    private static ClientWebSocket ws;
    private static string accessToken;
    private static JsonElement voiceSettings;
    private static JsonElement voiceChannel;
    private static string currentVoiceChannelId;
    private static int voiceChannelUserCount = 0;

    public static async Task Connect() {
        ws = new ClientWebSocket();
        ws.Options.SetRequestHeader("Origin", "http://localhost:3000");
        await ws.ConnectAsync(new Uri("ws://127.0.0.1:6463/?v=1&encoding=json&client_id=207646673902501888"),
            CancellationToken.None);

        // Start receiving messages after connection
        _ = ReceiveMessagesAsync();
    }

    private static async Task GetAccessTokenStage1() {
        var payload = new {
            cmd = "AUTHORIZE",
            args = new {
                client_id = "207646673902501888",
                scopes = new string[] { "rpc" },
                prompt = "none"
            },
            nonce = Guid.NewGuid().ToString("N"),
        };
        var buffer = Encoding.UTF8.GetBytes(System.Text.Json.JsonSerializer.Serialize(payload));
        await ws.SendAsync(new ArraySegment<byte>(buffer), WebSocketMessageType.Text, true, CancellationToken.None);
    }

    private static async Task GetAccessTokenStage2(string code) {
        const string url = "https://streamkit.discord.com/overlay/token";
        var payload = new { code };
        var request = new System.Net.Http.HttpRequestMessage(System.Net.Http.HttpMethod.Post, url);
        request.Content = new System.Net.Http.StringContent(System.Text.Json.JsonSerializer.Serialize(payload),
            Encoding.UTF8, "application/json");
        var response = await new System.Net.Http.HttpClient().SendAsync(request);
        var responseContent = await response.Content.ReadAsStringAsync();
        var responseJson = System.Text.Json.JsonDocument.Parse(responseContent).RootElement;
        accessToken = responseJson.GetProperty("access_token").GetString();
        if (accessToken == null) {
            Console.WriteLine("Failed to get access token");
            return;
        }

        await Authenticate();
    }

    private static async Task Authenticate() {
        var payload = new {
            cmd = "AUTHENTICATE",
            args = new {
                access_token = accessToken,
            },
            nonce = Guid.NewGuid().ToString("N"),
        };
        var buffer = Encoding.UTF8.GetBytes(System.Text.Json.JsonSerializer.Serialize(payload));
        await ws.SendAsync(new ArraySegment<byte>(buffer), WebSocketMessageType.Text, true, CancellationToken.None);
    }

    private static async Task SubscribeToEvents() {
        await SendSubscription("VOICE_CHANNEL_SELECT");
        await SendSubscription("VOICE_SETTINGS_UPDATE");
    }

    public static async Task ToggleMute() {
        if (voiceSettings.ValueKind == System.Text.Json.JsonValueKind.Undefined) {
            Console.WriteLine("Voice settings not available yet");
            return;
        }

        //set to opposite of current mute state
        await SendCommand("SET_VOICE_SETTINGS", new {
            mute = !voiceSettings.GetProperty("mute").GetBoolean(),
        });
    }

    public static async Task ToggleDeafen() {
        if (voiceSettings.ValueKind == System.Text.Json.JsonValueKind.Undefined) {
            Console.WriteLine("Voice settings not available yet");
            return;
        }

        //set to opposite of current mute state
        await SendCommand("SET_VOICE_SETTINGS", new {
            deaf = !voiceSettings.GetProperty("deaf").GetBoolean(),
        });
    }

    public static async Task LeaveChannel() {
        await SendCommand("SELECT_VOICE_CHANNEL", new { channel_id = (string)null, force = true });
    }


    public static async Task<JsonElement> GetInitialState() {
        if (voiceSettings.ValueKind == System.Text.Json.JsonValueKind.Undefined) {
            Console.WriteLine("Voice settings not available yet");
            return new JsonElement();
        }

        // Get mute and deafen state from voice settings
        var muteSetting = voiceSettings.GetProperty("mute").GetBoolean();
        var deafSetting = voiceSettings.GetProperty("deaf").GetBoolean();

        // Default values for channel properties
        var channelName = "";
        object users = "";

        // Update channel properties if voice channel is available
        if (voiceChannel.ValueKind != System.Text.Json.JsonValueKind.Undefined) {
            channelName = voiceChannel.GetProperty("name").GetString();
            users = voiceChannelUserCount;
        }

        // Create a single state object with all properties
        var state = new {
            cmd = "GET_INITIAL_STATE",
            mute = muteSetting,
            deaf = deafSetting,
            channelName,
            users
        };

        // Convert to JsonElement and return
        var jsonString = JsonSerializer.Serialize(state);
        return JsonDocument.Parse(jsonString).RootElement;
    }

    private static async Task SendSubscription(string evt, object args = null) {
        var payload = new {
            cmd = "SUBSCRIBE",
            evt,
            args,
            nonce = Guid.NewGuid().ToString("N"),
        };

        var buffer = Encoding.UTF8.GetBytes(System.Text.Json.JsonSerializer.Serialize(payload));
        await ws.SendAsync(new ArraySegment<byte>(buffer), WebSocketMessageType.Text, true, CancellationToken.None);
    }

    private static async Task SendCommand(string cmd, object args = null) {
        var payload = new {
            cmd,
            args,
            nonce = Guid.NewGuid().ToString("N"),
        };

        var buffer = Encoding.UTF8.GetBytes(System.Text.Json.JsonSerializer.Serialize(payload));
        await ws.SendAsync(new ArraySegment<byte>(buffer), WebSocketMessageType.Text, true, CancellationToken.None);
    }

    private static async Task SubscribeToVoiceStateEvents(string channelId) {
        await SendSubscription("VOICE_STATE_CREATE", new { channel_id = channelId });
        await SendSubscription("VOICE_STATE_DELETE", new { channel_id = channelId });
        Console.WriteLine($"Subscribed to voice state events for channel {channelId}");
    }

    private static async Task SendUnsubscription(string evt, object args = null) {
        var payload = new {
            cmd = "UNSUBSCRIBE",
            evt,
            args,
            nonce = Guid.NewGuid().ToString("N"),
        };

        var buffer = Encoding.UTF8.GetBytes(JsonSerializer.Serialize(payload));
        await ws.SendAsync(new ArraySegment<byte>(buffer), WebSocketMessageType.Text, true, CancellationToken.None);
    }

    private static async Task UnsubscribeFromVoiceStateEvents(string channelId) {
        await SendUnsubscription("VOICE_STATE_CREATE", new { channel_id = channelId });
        await SendUnsubscription("VOICE_STATE_DELETE", new { channel_id = channelId });
        Console.WriteLine($"Unsubscribed from voice state events for channel {channelId}");
    }

    private static async Task GetCurrentVoiceChannel() {
        await SendCommand("GET_SELECTED_VOICE_CHANNEL");
    }


    private static async Task ReceiveMessagesAsync() {
        var buffer = new byte[4096];
        try {
            while (ws.State == WebSocketState.Open) {
                var result = await ws.ReceiveAsync(new ArraySegment<byte>(buffer), CancellationToken.None);

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
                            var json = System.Text.Json.JsonDocument.Parse(message).RootElement;
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
                                                voiceChannelUserCount++;
                                                Console.WriteLine(
                                                    $"User joined voice channel. Total users: {voiceChannelUserCount}");
                                                await PebbleWSServer.UserNumberChange(voiceChannelUserCount);
                                                break;

                                            case "VOICE_STATE_DELETE":
                                                if (voiceChannelUserCount > 0) voiceChannelUserCount--;
                                                Console.WriteLine(
                                                    $"User left voice channel. Total users: {voiceChannelUserCount}");
                                                await PebbleWSServer.UserNumberChange(voiceChannelUserCount);
                                                break;

                                            case "VOICE_CHANNEL_SELECT":
                                                //we actually only get the voice channel id
                                                var channel_id = json.GetProperty("data").GetProperty("channel_id")
                                                    .GetString();

                                                // Handle unsubscribing from previous channel if we were in one
                                                if (currentVoiceChannelId != null &&
                                                    (channel_id == null || channel_id != currentVoiceChannelId)) {
                                                    await UnsubscribeFromVoiceStateEvents(currentVoiceChannelId);
                                                }

                                                if (channel_id == null) {
                                                    currentVoiceChannelId = null;
                                                    voiceChannel = new JsonElement();
                                                    PebbleWSServer.LeftChannel();
                                                }
                                                else {
                                                    // Subscribe to the new channel
                                                    currentVoiceChannelId = channel_id;
                                                    await SubscribeToVoiceStateEvents(channel_id);
                                                    SendCommand("GET_CHANNEL", new { channel_id });
                                                }

                                                break;

                                            case "VOICE_SETTINGS_UPDATE":
                                                voiceSettings = json.GetProperty("data");
                                                PebbleWSServer.UserVoiceStateUpdate(
                                                    voiceSettings.GetProperty("mute").GetBoolean(),
                                                    voiceSettings.GetProperty("deaf").GetBoolean());
                                                break;

                                            default:
                                                Console.WriteLine("Unhandled event: " + eventType);
                                                break;
                                        }

                                        break;
                                    case "GET_CHANNEL":
                                        voiceChannel = json.GetProperty("data");
                                        if (voiceChannel.TryGetProperty("voice_states", out var voiceStates)) {
                                            voiceChannelUserCount = voiceStates.GetArrayLength();
                                            Console.WriteLine(
                                                $"Updated voice channel user count: {voiceChannelUserCount}");
                                        }
                                        else {
                                            //voice channel user count is at least 1, because we are in the channel
                                            voiceChannelUserCount = 1;
                                        }

                                        PebbleWSServer.JoinedChannel(
                                            voiceChannel.GetProperty("name").GetString(),
                                            voiceChannelUserCount);
                                        break;

                                    case "GET_SELECTED_VOICE_CHANNEL":
                                        if (json.TryGetProperty("data", out var channelData)) {
                                            // Check if data is null
                                            if (channelData.ValueKind == JsonValueKind.Null) {
                                                Console.WriteLine("User is not in a voice channel");
                                                currentVoiceChannelId = null;
                                                voiceChannelUserCount = 0;
                                                await PebbleWSServer.LeftChannel();
                                            }
                                            else if (channelData.TryGetProperty("id", out var idElement)) {
                                                var channelId = idElement.GetString();
                                                if (!string.IsNullOrEmpty(channelId)) {
                                                    currentVoiceChannelId = channelId;
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
                                            accessToken = null;
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
                        await ws.CloseAsync(WebSocketCloseStatus.NormalClosure, "Connection closed by server",
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
        }
        catch (Exception ex) {
            Console.WriteLine($"WebSocket error: {ex.Message}");

            // Try to reconnect after a short delay
            await Task.Delay(5000);
            try {
                if (ws.State != WebSocketState.Open) {
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