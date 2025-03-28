#include "join_channel_window.h"

static Window *s_window;
static TextLayer *s_instruction_layer;
static Layer *s_discord_icon_layer;
static GDrawCommandImage *s_discord_icon;

static void discord_icon_layer_update_proc(Layer *layer, GContext *ctx) {
  if (!s_discord_icon) return;
  
  // Draw the Discord icon centered
  GRect bounds = layer_get_bounds(layer);
  int x_center = (bounds.size.w - 80) / 2; // 50 is the icon width
  gdraw_command_image_draw(ctx, s_discord_icon, GPoint(x_center, 0));
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  #if PBL_COLOR
    window_set_background_color(window, GColorIndigo);
  #endif
  
  // Calculate dimensions for centered content
  int available_height = bounds.size.h;
  int total_content_height = 160; // 50px for icon + 20px spacing + 60px for text
  int y_offset = (available_height - total_content_height) / 2;
  
  // Create Discord icon layer
  s_discord_icon = gdraw_command_image_create_with_resource(RESOURCE_ID_DISCORD_80);
  s_discord_icon_layer = layer_create(GRect(0, y_offset, bounds.size.w, 80));
  layer_set_update_proc(s_discord_icon_layer, discord_icon_layer_update_proc);
  layer_add_child(window_layer, s_discord_icon_layer);
  
  // Create the text layer with instructions
  int text_width = bounds.size.w - 20;
  s_instruction_layer = text_layer_create(GRect(10, y_offset + 80, text_width, 60));
  
  text_layer_set_text(s_instruction_layer, "Join a Voice Channel on your PC");
  text_layer_set_text_alignment(s_instruction_layer, GTextAlignmentCenter);
  
  #if PBL_DISPLAY_HEIGHT == 228
    text_layer_set_font(s_instruction_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  #else
    text_layer_set_font(s_instruction_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  #endif
  
  #if PBL_COLOR
    text_layer_set_text_color(s_instruction_layer, GColorWhite);
    text_layer_set_background_color(s_instruction_layer, GColorClear);
  #endif
  
  layer_add_child(window_layer, text_layer_get_layer(s_instruction_layer));
}

static void window_unload(Window *window) {
  if (s_instruction_layer) {
    text_layer_destroy(s_instruction_layer);
  }
  
  if (s_discord_icon) {
    gdraw_command_image_destroy(s_discord_icon);
  }
  
  if (s_discord_icon_layer) {
    layer_destroy(s_discord_icon_layer);
  }
  
  window_destroy(s_window);
  s_window = NULL;
}

void join_channel_window_push() {
  if (!s_window) {
    s_window = window_create();
    window_set_window_handlers(s_window, (WindowHandlers) {
      .load = window_load,
      .unload = window_unload
    });

  }
  
  window_stack_push(s_window, true);
}

Window* join_channel_window_get_window() {
    return s_window;
  }

void join_channel_window_pop() {
  if (s_window) {
    window_stack_remove(s_window, true);
  }
}