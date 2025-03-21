# Discord Companion Desktop Server

### NOTE: This is the original hackathon version of discord companion. Checkout the main branch for a newer version (you have to compile the Pebble App yourself)
This README explains how to set up and run the Discord companion WebSocket server that enables control of Discord voice settings from a Pebble smartwatch application.

## Prerequisites

- [Node.js](https://nodejs.org/) (version 22 or newer recommended)
- Discord desktop application installed

## Setup Instructions

### 1. Clone the repository

```bash
git clone https://github.com/jplexer/discord-companion
cd discord-companion/desktop
```

### 2. Install dependencies

```bash
npm install
```

### 3. Configure environment variables

Copy the example environment file and add your Discord application credentials:

```bash
cp .env.example .env
```

Edit the `.env` file and add the following:

```
DISCORD_CLIENT_ID=your_discord_application_id
DISCORD_CLIENT_SECRET=your_discord_application_secret
```

You can get these credentials by creating a Discord application at the [Discord Developer Portal](https://discord.com/developers/applications).

### 4. Run the server

```bash
node index.js
```

The server will start and display the WebSocket URL (e.g., `ws://192.168.1.100:5983`) that the Pebble app can connect to.

## Troubleshooting

- Make sure Discord is running before starting the server
- The first time you run the application, you'll need to authorize the Discord RPC connection
