#pragma once

#include <pebble.h>

void error_window_push(const char *message);

void error_window_pop(void);

Window* error_window_get_window(void);