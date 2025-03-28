#include "error_window.h"

static Window *s_window;
static TextLayer *s_error_text_layer;
static GDrawCommandImage *s_warning_icon;
static Layer *s_warning_icon_layer;

static char s_error[128];

static void warning_icon_update_proc(Layer *layer, GContext *ctx) {
  if (!s_warning_icon) return;
  
  // Draw the warning icon at the origin of the layer
  gdraw_command_image_draw(ctx, s_warning_icon, GPoint(0, 0));
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  #if PBL_COLOR
    window_set_background_color(window, GColorLightGray);
  #endif

  // Calculate positions for icon and text
  int icon_margin, icon_y, text_y, text_width, text_margin;
  GTextAlignment text_alignment;
  
  #if PBL_ROUND
    icon_margin = bounds.size.w / 2 - 12;
    text_margin = 20;
    icon_y = 25;
    text_y = icon_y + 25; // Below icon with spacing
    text_width = bounds.size.w - 40;
    text_alignment = GTextAlignmentCenter;
  #else
    icon_margin = 5;
    text_margin = icon_margin;
    icon_y = 10;
    text_y = icon_y + 25; // Below icon with spacing
    text_width = bounds.size.w - 10;
    text_alignment = GTextAlignmentLeft;
  #endif

  // Create warning icon and layer
  s_warning_icon = gdraw_command_image_create_with_resource(RESOURCE_ID_ICON_WARNING);
  s_warning_icon_layer = layer_create(GRect(icon_margin, icon_y, 25, 25));
  layer_set_update_proc(s_warning_icon_layer, warning_icon_update_proc);
  
  // Now create the text layer AFTER calculating positions
  s_error_text_layer = text_layer_create(GRect(text_margin, text_y, text_width, 120));
  
  // Set text properties AFTER creating the layer
  text_layer_set_text_alignment(s_error_text_layer, text_alignment);
  
  #if PBL_DISPLAY_HEIGHT == 228
    text_layer_set_font(s_error_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  #else
    text_layer_set_font(s_error_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  #endif

  text_layer_set_text(s_error_text_layer, s_error);
  text_layer_set_overflow_mode(s_error_text_layer, GTextOverflowModeWordWrap);
  text_layer_set_background_color(s_error_text_layer, GColorClear);

  // Add layers to window
  layer_add_child(window_layer, text_layer_get_layer(s_error_text_layer));
  layer_add_child(window_layer, s_warning_icon_layer);
}

static void window_unload(Window *window) {
  // Clean up resources
  if (s_error_text_layer) {
    text_layer_destroy(s_error_text_layer);
  }
  
  if (s_warning_icon_layer) {
    layer_destroy(s_warning_icon_layer);
  }
  
  if (s_warning_icon) {
    gdraw_command_image_destroy(s_warning_icon);
  }
  
  window_destroy(s_window);
  s_window = NULL;
}

void error_window_push(const char *message) {
  // Store title and message
  strncpy(s_error, message, sizeof(s_error) - 1);
  s_error[sizeof(s_error) - 1] = '\0';
  
  // Create window if needed
  if (!s_window) {
    s_window = window_create();
    window_set_window_handlers(s_window, (WindowHandlers) {
      .load = window_load,
      .unload = window_unload,
    });
  }
  
  window_stack_push(s_window, true);
}
  
void error_window_pop() {
  if(s_window) {
    window_stack_remove(s_window, true);
  }
}