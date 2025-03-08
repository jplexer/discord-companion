var keys = require('message_keys');
var Clay = require('pebble-clay');
var clayConfig = require('./config.json');
var clay = new Clay(clayConfig);
var websocketHost = "";
var websocketPort = 5983;
var websocketUrl = "ws://" + websocketHost + ":" + websocketPort;
var initialized = false;

Pebble.addEventListener("ready",
    function(e) {
        var tempHost = localStorage.getItem("WS_HOST");
        var tempPort = localStorage.getItem("WS_PORT");
        if (!tempHost) {
            // we cant run in this state, so we need to exit
            // the user will need to configure the app
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

// Global WebSocket connection
let socket = null;

// Polling timer IDs
let statePollingTimer = null;
let voiceInfoPollingTimer = null;

// Polling intervals (in milliseconds)
const STATE_POLLING_INTERVAL = 5000;       // 5 seconds
const VOICE_INFO_POLLING_INTERVAL = 30000; // 30 seconds

// Cache of the last sent values to avoid unnecessary updates
const lastSentValues = {
    MUTE_STATE: null,
    DEAFEN_STATE: null,
    VOICE_CHANNEL_NAME: null,
    VOICE_USER_COUNT: null
};

function initWebSocket() {
    // Clear any existing timers if reconnecting
    stopAllPolling();
    
    // Replace with your WebSocket server address
    const wsUrl = websocketUrl; 
    
    try {
        socket = new WebSocket(wsUrl);
        
        socket.onopen = function(e) {
            console.log("WebSocket connection established");
            
            // Request initial state as soon as connection is established
            setTimeout(function() {
                requestStateFromServer();
                requestVoiceInfoFromServer();
                
                // Start polling once initial requests are sent
                startPolling();
            }, 500); // Short delay to let the connection stabilize
        };
        
        socket.onmessage = function(event) {
            console.log("Message from server received");
            
            // Handle Blob responses
            if (event.data instanceof Blob) {
                const reader = new FileReader();
                reader.onload = function() {
                    handleMessageData(reader.result);
                };
                reader.readAsText(event.data);
            } else {
                // Handle text responses directly
                handleMessageData(event.data);
            }
        };
        
        socket.onclose = function(event) {
            console.log('WebSocket connection closed');
            
            // Stop polling when connection closes
            stopAllPolling();
            
            // Attempt to reconnect after a delay
            setTimeout(initWebSocket, 5000);
        };
        
        socket.onerror = function(error) {
            console.log("WebSocket error occurred");
            // Stop polling on error
            stopAllPolling();
        };
        
    } catch(err) {
        console.log("WebSocket connection error: " + err.message);
        stopAllPolling();
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
        
        // Handle voice info JSON object
        if (jsonData && typeof jsonData === 'object' && 'name' in jsonData) {
            console.log("Received voice info:", jsonData);
            
            // Send voice info to Pebble (only if changed)
            sendStateToPebble({
                VOICE_CHANNEL_NAME: jsonData.name || "Unknown channel",
                VOICE_USER_COUNT: jsonData.userCount || 0
            });
            return;
        }
        
        // Handle JSON array format [muteState, deafState]
        if (Array.isArray(jsonData) && jsonData.length === 2) {
            const muteState = jsonData[0];
            const deafState = jsonData[1];
            
            console.log("Received state - Mute:", muteState, "Deafen:", deafState);
            
            // Send both states in a single message (only if changed)
            sendStateToPebble({
                MUTE_STATE: muteState ? 1 : 0,
                DEAFEN_STATE: deafState ? 1 : 0
            });
        } else {
            console.log("Received unexpected JSON format:", jsonData);
        }
    } catch (e) {
        console.log("Error parsing JSON:", e.message, "Data:", JSON.stringify(data));
        
        // Handle as string if JSON parsing fails
        const dataStr = String(data);
        
        if (dataStr === "mutetrue") {
            sendStateToPebble({MUTE_STATE: 1});
        } else if (dataStr === "mutefalse") {
            sendStateToPebble({MUTE_STATE: 0});
        } else if (dataStr === "deafentrue") {
            sendStateToPebble({DEAFEN_STATE: 1});
        } else if (dataStr === "deafenfalse") {
            sendStateToPebble({DEAFEN_STATE: 0});
        } else {
            console.log("Unrecognized message format:", dataStr);
        }
    }
}

function sendStateToPebble(state) {
    // Create a copy of the state that only includes changed values
    const changedState = {};
    let hasChanges = false;
    
    // Check each property for changes
    Object.keys(state).forEach(key => {
        if (state[key] !== lastSentValues[key]) {
            changedState[key] = state[key];
            lastSentValues[key] = state[key];
            hasChanges = true;
        }
    });
    
    // Only send message if there are actual changes
    if (hasChanges) {
        console.log("Sending changed state to Pebble:", JSON.stringify(changedState));
        Pebble.sendAppMessage(changedState, 
            function() {
                console.log("Successfully sent state to Pebble");
            },
            function(e) {
                console.log("Failed to send state to Pebble:", JSON.stringify(e));
                // Reset last sent values for failed properties to force retry next time
                Object.keys(changedState).forEach(key => {
                    lastSentValues[key] = null;
                });
            }
        );
    } else {
        console.log("No changes detected, skipping update to save energy");
    }
}

// Modified to avoid the timeout if polling is active
function requestStateFromServer() {
    if (!socket) {
        console.log("No WebSocket connection exists");
        return;
    }
    
    if (socket.readyState === WebSocket.OPEN) {
        console.log("Requesting state from server");
        socket.send("getState");
        socket.send("getVoiceInfo");
    } else if (socket.readyState === WebSocket.CONNECTING) {
        console.log("WebSocket still connecting, waiting before requesting state");
    } else {
        console.log("WebSocket not connected (state: " + socket.readyState + "), cannot request state");
        // Try to reconnect - only if we're not already reconnecting
        if (socket.readyState !== WebSocket.CONNECTING) {
            stopAllPolling(); // Stop polling during reconnection attempt
            initWebSocket();
        }
    }
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
        // Check if we received the request state message
        else if (e.payload && e.payload.REQUEST_STATE !== undefined) {
            requestStateFromServer();
        }
        // Check for voice info request
        else if (e.payload && e.payload.REQUEST_VOICE_INFO !== undefined) {
            requestVoiceInfoFromServer();
        }
    }
);

function sendMuteCommand() {
    if (socket && socket.readyState === WebSocket.OPEN) {
        console.log("Sending mute command to server");
        socket.send("mute");
    } else {
        console.log("WebSocket not connected, cannot send mute command");
    }
}

function sendDeafenCommand() {
    if (socket && socket.readyState === WebSocket.OPEN) {
        console.log("Sending deafen command to server");
        socket.send("deafen");
    } else {
        console.log("WebSocket not connected, cannot send deafen command");
    }
}

// Modified voice info request to handle connection states
function requestVoiceInfoFromServer() {
    if (!socket) {
        console.log("No WebSocket connection exists");
        return;
    }
    
    if (socket.readyState === WebSocket.OPEN) {
        console.log("Requesting voice info from server");
        socket.send("getVoiceInfo");
    } else {
        console.log("WebSocket not connected, cannot request voice info");
    }
}

// Start periodic polling for updates
function startPolling() {
    // Only start if not already polling
    if (!statePollingTimer) {
        console.log("Starting state polling every " + (STATE_POLLING_INTERVAL/1000) + " seconds");
        statePollingTimer = setInterval(requestStateFromServer, STATE_POLLING_INTERVAL);
    }
    
    if (!voiceInfoPollingTimer) {
        console.log("Starting voice info polling every " + (VOICE_INFO_POLLING_INTERVAL/1000) + " seconds");
        voiceInfoPollingTimer = setInterval(requestVoiceInfoFromServer, VOICE_INFO_POLLING_INTERVAL);
    }
}

// Stop all polling timers
function stopAllPolling() {
    if (statePollingTimer) {
        console.log("Stopping state polling");
        clearInterval(statePollingTimer);
        statePollingTimer = null;
    }
    
    if (voiceInfoPollingTimer) {
        console.log("Stopping voice info polling");
        clearInterval(voiceInfoPollingTimer);
        voiceInfoPollingTimer = null;
    }
}

// Make sure to clean up when the app closes
Pebble.addEventListener("unload", function() {
    console.log("App is closing, cleaning up resources");
    stopAllPolling();
    if (socket) {
        socket.close();
    }
});