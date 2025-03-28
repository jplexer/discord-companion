# Discord Companion Desktop Server

This README explains how to set up and run the Discord companion WebSocket server that enables control of Discord voice settings from a Pebble smartwatch application.

## Prerequisites
- Discord desktop application installed

## Setup Instructions

### Option 1: Download the Latest Release

1. Go to the [Releases page](https://github.com/jplexer/discord-companion/releases) of this repository
2. Download the latest release package for your Platform (AppImage for Linux, Windows for Windows)
3. Extract the downloaded zip file (Windows only)
4. Run the executable file (e.g., `Pebble Companion.exe`)

### Option 2: Download from GitHub Actions

1. Go to the [GitHub Actions](https://github.com/jplexer/discord-companion/actions) tab of this repository
2. Select the most recent successful workflow run
3. Download the desktop server artifact from the Artifacts section
4. Extract the downloaded zip file
5. Run the executable file (e.g., `Pebble Companion.exe`)

### Option 3: Self-compile

1. Navigate to the `desktop` folder in this repository
2. Open the solution file (`.sln`) in Rider or your preferred C# IDE
3. Restore NuGet packages
4. Build the solution
5. Run the compiled executable from the output directory

## Troubleshooting

- Make sure Discord is running before starting the server
- The first time you run the application, you'll need to authorize the Discord RPC connection
