# Discord Companion Desktop Server

### NOTE: This is the original hackathon version of discord companion. Checkout the main branch for a newer version (you have to compile the Pebble App yourself)
This README explains how to set up and run the Discord companion WebSocket server that enables control of Discord voice settings from a Pebble smartwatch application.

## Prerequisites
- Discord desktop application installed

## Setup Instructions

### Option 1: Download from GitHub Actions

1. Go to the [GitHub Actions](https://github.com/jplexer/discord-companion/actions) tab of this repository
2. Select the most recent successful workflow run
3. Download the desktop server artifact from the Artifacts section
4. Extract the downloaded zip file
5. Run the executable file (e.g., `DiscordCompanionServer.exe`)

### Option 2: Self-compile

1. Navigate to the `desktop` folder in this repository
2. Open the solution file (`.sln`) in Rider or your preferred C# IDE
3. Restore NuGet packages
4. Build the solution
5. Run the compiled executable from the output directory

## Troubleshooting

- Make sure Discord is running before starting the server
- The first time you run the application, you'll need to authorize the Discord RPC connection
