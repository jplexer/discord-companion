#pragma once

#include <pebble.h>

// Initialize and push the loading window to the window stack
void loading_window_push(void);

// Close the loading window
void loading_window_pop(void);