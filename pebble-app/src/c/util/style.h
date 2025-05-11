#include <pebble.h>

// ----- Colors -----
#define ACCENT_COLOR GColorIndigo

#define BACKGROUND_COLOR PBL_IF_COLOR_ELSE(ACCENT_COLOR, GColorWhite)
#define FOREGROUND_COLOR PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack)

// ----- Margins --- --
#if PBL_DISPLAY_WIDTH >= 200
  #define HORIZONTAL_GUTTERS 12
#else
  #define HORIZONTAL_GUTTERS 10 // Distance from text to left edge of screen, and 
#endif                          // on rect, from text to left edge of ActionBar

#if defined(PBL_ROUND)
  #define ROUND_ACTION_BAR_GUTTER 14 // Round gets extra space between text and
#endif                               // ActionBar

#if PBL_DISPLAY_WIDTH >= 200
  #define MARGIN_BETWEEN_TEXT 3
#else
  #define MARGIN_BETWEEN_TEXT 2 // Distance from bottom of a text block to
#endif                          // top of the next text block

#define MARGIN_ABOVE_BELOW_TEXT 8 // On rect, distance from bottom of StatusBar
                                  // to top of first text block, or on round,
                                  // from bottom of last text block to top of
                                  // discord icon

#if PBL_DISPLAY_HEIGHT >= 228
  #define MARGIN_BELOW_ICON 8
#elif defined(PBL_ROUND)
  #define MARGIN_BELOW_ICON 6
#else
  #define MARGIN_BELOW_ICON 4 // Distance between discord icon and
#endif                        // bottom edge of screen