#pragma once

#include <pebble.h>

// Initialize and push the join channel window to the window stack
void join_channel_window_push(void);

// Close the join channel window
void join_channel_window_pop(void);

Window* join_channel_window_get_window(void);