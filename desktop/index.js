import { Client } from "@xhayper/discord-rpc";
import { WebSocketServer } from "ws";
import dotenv from "dotenv";
import os from "os";
dotenv.config();

const wss = new WebSocketServer({ port: 5983 });

const client = new Client({
  clientId: process.env.DISCORD_CLIENT_ID,
  clientSecret: process.env.DISCORD_CLIENT_SECRET,
});

var clientReady = false;

client.on("ready", async () => {
  clientReady = true;
  // get ip of the machine in the network
  const ifaces = os.networkInterfaces();
  let ip = "";
  Object.keys(ifaces).forEach((ifname) => {
    ifaces[ifname].forEach((iface) => {
      if ("IPv4" !== iface.family || iface.internal !== false) {
        return;
      }
      ip = iface.address;
    });
  });
  console.log(`Listening on ws://${ip}:5983`);
});

wss.on("connection", (ws) => {

  ws.on("message", async (message) => {
    if (!clientReady) {
      ws.send("Discord client not ready");
      return;
    }
    const messageString = message.toString();
    switch (messageString) {
      case "mute":
        const muteResult = await toggleMute();
        ws.send(muteResult.toString());
        break;
      case "deafen":
        const deafenResult = await toggleDeafen();
        ws.send(deafenResult.toString());
        break;
      case "getState":
        var voiceSettings = await client.user?.getVoiceSettings();
        var muteState = voiceSettings.mute ? 1 : 0;
        var deafState = voiceSettings.deaf ? 1 : 0;
        ws.send(JSON.stringify([muteState, deafState]));
        break;
      case "getVoiceInfo":
        var voiceChannel = await client.user?.getSelectedVoiceChannel();
        var voiceName = voiceChannel?.name || "Not in a voice channel";
        var userCount = voiceChannel?.voice_states?.length || 0;
        ws.send(
          JSON.stringify({
            name: voiceName,
            userCount: userCount,
          }),
        );
        break;
      default:
        ws.send("Invalid command");
        break;
    }
  });

  ws.on("close", () => {
  });

  ws.on("error", (error) => {
    console.error(`WebSocket error: ${error}`);
  });
});

async function toggleMute() {
  var voiceSettings = await client.user?.getVoiceSettings();
  client.user?.setVoiceSettings({
    mute: !voiceSettings.mute,
  });
  return "mute" + !voiceSettings.mute;
}

async function toggleDeafen() {
  var voiceSettings = await client.user?.getVoiceSettings();
  client.user?.setVoiceSettings({
    deaf: !voiceSettings.deaf,
  });
  return "deafen" + !voiceSettings.deaf;
}

client.login({
  scopes: ["rpc"],
  prompt: "none", // Only prompt once
});
