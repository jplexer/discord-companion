#pragma once

#include <pebble.h>

void main_window_push();

void main_window_pop();

bool main_window_is_in_channel();

// Add this declaration
Window* main_window_get_window(void);

void main_window_update_voice_info(const char* channel_name, int user_count, const char* topic);