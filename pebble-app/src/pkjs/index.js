var keys = require('message_keys');
var Clay = require('pebble-clay');
var clayConfig = require('./config.json');
var clay = new Clay(clayConfig);
var websocketHost = "";
var websocketPort = 5983;
var websocketUrl = "ws://" + websocketHost + ":" + websocketPort;
var initialized = false;
var watchInfo = Pebble.getActiveWatchInfo();

let connectionStartTime = 0;
const MAX_RETRY_TIME = 10000; // 10 seconds in milliseconds
let isRetrying = false;

Pebble.addEventListener("ready",
    function(e) {
        console.log("Watch Model: " + watchInfo.model);
        if (watchInfo.model.startsWith("qemu")) {
            console.log("Running in emulator, skipping WebSocket initialization");
            sendStateToPebble({
                VOICE_CHANNEL_NAME: "Fake Channel and some more characters",
                VOICE_USER_COUNT: 42,
                VOICE_SERVER_NAME: "qemu",
                MUTE_STATE: 0,
                DEAFEN_STATE: 0
            });
            return;
        }
        var tempHost = localStorage.getItem("WS_HOST");
        var tempPort = localStorage.getItem("WS_PORT");
        //tempHost = "192.168.0.231"; //override for testing
        if (!tempHost) {
            // we cant run in this state, so we need to exit
            // the user will need to configure the app
            
            // Send disconnected status to show loading screen
            console.log("No WebSocket host configured, sending disconnected status to Pebble");
            sendConnectionStatus(false);
            return;
        }
        websocketHost = tempHost;
        if (tempPort != null) {
            websocketPort = tempPort;
        }
        websocketUrl = "ws://" + websocketHost + ":" + websocketPort;
        console.log("WebSocket URL: " + websocketUrl);
        initialized = true;
        // Initialize WebSocket connection when Pebble app starts
        initWebSocket();
    }
);

Pebble.addEventListener("showConfiguration", function(e) {
    var url = clay.generateUrl();
    Pebble.openURL(url);
  });
  
  Pebble.addEventListener('webviewclosed', function(e) {
    if (e && !e.response) {
      return;
    }
  
    // Get the keys and values from each config item
    var dict = clay.getSettings(e.response);
    wsHost = dict[keys.WS_HOST];
    localStorage.setItem("WS_HOST", wsHost);
    websocketHost = wsHost;
    wsPort = dict[keys.WS_PORT];
    websocketPort = wsPort;
    localStorage.setItem("WS_PORT", wsPort);
    websocketUrl = "ws://" + websocketHost + ":" + websocketPort;
    if (!initialized) {
      initialized = true;
      initWebSocket();
    }
  });

// Function to send connection status to Pebble
function sendConnectionStatus(isConnected) {
    console.log("Sending connection status to Pebble: " + (isConnected ? "Connected" : "Disconnected"));
    
    Pebble.sendAppMessage({
        CONNECTION_STATUS: isConnected ? 1 : 0
    }, 
    function() {
        console.log("Successfully sent connection status");
    },
    function(e) {
        console.log("Failed to send connection status: " + JSON.stringify(e));
    });
}

// Global WebSocket connection
let socket = null;

// Cache of the last sent values to avoid unnecessary updates
const lastSentValues = {
    MUTE_STATE: null,
    DEAFEN_STATE: null,
    VOICE_CHANNEL_NAME: null,
    VOICE_USER_COUNT: null
};

function initWebSocket() {
    // Set connection start time when first attempting to connect
    if (!isRetrying) {
        connectionStartTime = Date.now();
        isRetrying = true;
    }
    
    // Check if we've exceeded retry time limit
    if (Date.now() - connectionStartTime > MAX_RETRY_TIME) {
        console.log("Exceeded maximum retry time of 10 seconds, stopping reconnection attempts");
        isRetrying = false;
        
        // Send final disconnected status to Pebble
        sendConnectionStatus(false);
        
        // Send a special message to indicate timeout (the watch app should show error window)
        Pebble.sendAppMessage({
            CONNECTION_TIMEOUT: 1
        });
        
        return;
    }
    
    // Initial connection status - disconnected
    sendConnectionStatus(false);
    
    // Replace with your WebSocket server address
    const wsUrl = websocketUrl; 
    
    try {
        socket = new WebSocket(wsUrl);
        
        socket.onopen = function(e) {
            console.log("WebSocket connection established");
            
            // Reset retry flag since we're now connected
            isRetrying = false;
            
            // Send connected status to Pebble
            sendConnectionStatus(true);
            
            // Request initial state as soon as connection is established
            setTimeout(function() {
                requestInitialStateFromServer();
            }, 500); // Short delay to let the connection stabilize
        };
        
        socket.onmessage = function(event) {
            console.log("Message from server received");
            handleMessageData(event.data);
        };
        
        socket.onclose = function(event) {
            console.log('WebSocket connection closed');
            
            // Send disconnected status to Pebble
            sendConnectionStatus(false);
            
            // Check if we should retry based on time
            if (Date.now() - connectionStartTime <= MAX_RETRY_TIME) {
                console.log("Retrying connection... Time elapsed: " + 
                          (Date.now() - connectionStartTime) + "ms");
                // Attempt to reconnect after a delay
                setTimeout(initWebSocket, 2000);
            } else {
                console.log("Exceeded maximum retry time, stopping reconnection attempts");
                isRetrying = false;
                
                // Send timeout message to Pebble
                Pebble.sendAppMessage({
                    CONNECTION_TIMEOUT: 1
                });
            }
        };
        
        socket.onerror = function(error) {
            console.log("WebSocket error occurred");
            
            // Send disconnected status to Pebble
            sendConnectionStatus(false);
        };
        
    } catch(err) {
        console.log("WebSocket connection error: " + err.message);
        
        // Send disconnected status to Pebble
        sendConnectionStatus(false);
        
        // Check if we should retry based on time
        if (Date.now() - connectionStartTime <= MAX_RETRY_TIME) {
            console.log("Retrying connection after error... Time elapsed: " + 
                      (Date.now() - connectionStartTime) + "ms");
            // Attempt to reconnect after a delay
            setTimeout(initWebSocket, 2000);
        } else {
            console.log("Exceeded maximum retry time after error, stopping reconnection attempts");
            isRetrying = false;
            
            // Send timeout message to Pebble
            Pebble.sendAppMessage({
                CONNECTION_TIMEOUT: 1
            });
        }
    }
}

// Helper function to process message data
function handleMessageData(data) {
    console.log("Processing message data:", data);
    
    // Check for empty/null data first
    if (!data || data.trim() === '') {
        console.log("Received empty data from WebSocket server");
        return;
    }
    
    try {
        // Try to parse as JSON
        const jsonData = JSON.parse(data);
        if (!jsonData) {
            console.log("Received invalid JSON data:", data);
            return;
        }

        switch (jsonData.cmd) {
            case "GET_INITIAL_STATE":
                sendStateToPebble({
                    VOICE_CHANNEL_NAME: jsonData.channelName,
                    VOICE_USER_COUNT: jsonData.users,
                    VOICE_SERVER_NAME: jsonData.serverName,
                    MUTE_STATE: jsonData.mute ? 1 : 0,
                    DEAFEN_STATE: jsonData.deaf ? 1 : 0
                });
                break;
            case "USER_VOICE_STATE_UPDATE":
                sendStateToPebble({
                    MUTE_STATE: jsonData.mute ? 1 : 0,
                    DEAFEN_STATE: jsonData.deaf ? 1 : 0
                });
                break;
            case "USER_NUMBER_CHANGE":
                sendStateToPebble({
                    VOICE_USER_COUNT: jsonData.userNumber
                });
                break;
            case "JOINED_CHANNEL":
                sendStateToPebble({
                    VOICE_CHANNEL_NAME: jsonData.channelName,
                    VOICE_USER_COUNT: jsonData.userNumber
                });
                break;
            case "SERVER_NAME_UPDATE":
                sendStateToPebble({
                    VOICE_SERVER_NAME: jsonData.serverName
                });
                break;
            case "LEFT_CHANNEL":
                sendStateToPebble({
                    VOICE_CHANNEL_NAME: "",
                    VOICE_USER_COUNT: 0,
                });
                break;
            default:
                console.log("Unknown command:", jsonData.cmd);
                break;
        }
        
    
    } catch (e) {
        console.log("Error parsing JSON:", e.message, "Data:", JSON.stringify(data));
    }
}

function sendStateToPebble(state) {

        Pebble.sendAppMessage(state, 
            function() {
                console.log("Successfully sent state to Pebble");
            },
            function(e) {
                console.log("Failed to send state to Pebble:", JSON.stringify(e));
            }
        );
}


// Listen for AppMessages from the Pebble
Pebble.addEventListener("appmessage",
    function(e) {
        console.log("AppMessage received: " + JSON.stringify(e.payload));
        
        // Check if we received the toggleMute message
        if (e.payload && e.payload.TOGGLE_MUTE !== undefined) {
            sendMuteCommand();
        }
        // Check if we received the toggleDeafen message
        else if (e.payload && e.payload.TOGGLE_DEAFEN !== undefined) {
            sendDeafenCommand();
        }
        // Check if we received the leaveChannel message
        else if (e.payload && e.payload.LEAVE_CHANNEL !== undefined) {
            sendLeaveChannelCommand();
        }
    }
);

var qemu_mute_state = 0;
function sendMuteCommand() {
    if (watchInfo.model.startsWith("qemu")) {
        console.log("Running in emulator, skipping mute command");
        sendStateToPebble({
            MUTE_STATE: qemu_mute_state ? 0 : 1
        });
        qemu_mute_state = !qemu_mute_state;
        return;
    }
    if (socket && socket.readyState === WebSocket.OPEN) {
        console.log("Sending mute command to server");
        socket.send("mute");
    } else {
        console.log("WebSocket not connected, cannot send mute command");
    }
}

var qemu_deafen_state = 0;
function sendDeafenCommand() {
    if (watchInfo.model.startsWith("qemu")) {
        console.log("Running in emulator, skipping deafen command");
        sendStateToPebble({
            DEAFEN_STATE: qemu_deafen_state ? 0 : 1
        });
        qemu_deafen_state = !qemu_deafen_state;
        return;
    }
    if (socket && socket.readyState === WebSocket.OPEN) {
        console.log("Sending deafen command to server");
        socket.send("deafen");
    } else {
        console.log("WebSocket not connected, cannot send deafen command");
    }
}

function sendLeaveChannelCommand() {
    if (socket && socket.readyState === WebSocket.OPEN) {
        console.log("Sending leave channel command to server");
        socket.send("leaveChannel");
    } else {
        console.log("WebSocket not connected, cannot send leave channel command");
    }
}

function requestInitialStateFromServer() {
    if (socket && socket.readyState === WebSocket.OPEN) {
        console.log("Requesting initial state from server");
        socket.send("getInitialState");
    } else {
        console.log("WebSocket not connected, cannot request initial state");
    }
}


// Make sure to clean up when the app closes
Pebble.addEventListener("unload", function() {
    console.log("App is closing, cleaning up resources");
    if (socket) {
        socket.close();
    }
});