#include <pebble.h>

#define ACCENT_COLOR GColorIndigo

#define BACKGROUND_COLOR PBL_IF_COLOR_ELSE(ACCENT_COLOR, GColorWhite)
#define FOREGROUND_COLOR PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack)