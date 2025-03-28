#include "loading_window.h"

static Window *s_window;
static TextLayer *s_loading_text_layer;
static GDrawCommandImage *s_discord_icon;
static Layer *s_discord_layer;

// Animation variables
static AppTimer *s_animation_timer;
static int s_loading_dots = 0;
static char s_loading_text[16] = "Connecting";

// Update loading text with animated dots
static void update_loading_text() {
  switch (s_loading_dots) {
    case 0:
      strcpy(s_loading_text, "Connecting");
      break;
    case 1:
      strcpy(s_loading_text, "Connecting.");
      break;
    case 2:
      strcpy(s_loading_text, "Connecting..");
      break;
    case 3:
      strcpy(s_loading_text, "Connecting...");
      break;
  }
  text_layer_set_text(s_loading_text_layer, s_loading_text);
  s_loading_dots = (s_loading_dots + 1) % 4;
}

static void animation_timer_callback(void *context) {
  update_loading_text();
  s_animation_timer = app_timer_register(500, animation_timer_callback, NULL);
}

static void discord_layer_update_proc(Layer *layer, GContext *ctx) {
  if (!s_discord_icon) return;
  gdraw_command_image_draw(ctx, s_discord_icon, GPoint(0, 0));
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  #if PBL_COLOR
    window_set_background_color(window, GColorIndigo);
  #endif

  // Create loading text layer
  s_loading_text_layer = text_layer_create(GRect(0, bounds.size.h/2 - 20, bounds.size.w, 40));
  text_layer_set_text_alignment(s_loading_text_layer, GTextAlignmentCenter);
  text_layer_set_font(s_loading_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text(s_loading_text_layer, s_loading_text);
  
  #if PBL_COLOR
    text_layer_set_text_color(s_loading_text_layer, GColorWhite);
    text_layer_set_background_color(s_loading_text_layer, GColorClear);
  #endif
  
  layer_add_child(window_layer, text_layer_get_layer(s_loading_text_layer));
  
  // Load and display Discord logo at the top
  s_discord_icon = gdraw_command_image_create_with_resource(RESOURCE_ID_DISCORD_50);
  
  // Center the Discord icon
  GRect discord_frame = GRect((bounds.size.w - 50)/2, bounds.size.h/4 - 25, 50, 50);
  
  s_discord_layer = layer_create(discord_frame);
  layer_set_update_proc(s_discord_layer, discord_layer_update_proc);
  layer_add_child(window_layer, s_discord_layer);
  
  // Start animation
  s_animation_timer = app_timer_register(500, animation_timer_callback, NULL);
}

static void window_unload(Window *window) {
  // Cancel any running timers
  if (s_animation_timer) {
    app_timer_cancel(s_animation_timer);
  }
  
  // Free resources
  text_layer_destroy(s_loading_text_layer);
  
  if (s_discord_layer) {
    layer_destroy(s_discord_layer);
  }
  
  if (s_discord_icon) {
    gdraw_command_image_destroy(s_discord_icon);
  }
  
  window_destroy(s_window);
  s_window = NULL;
}

void loading_window_push() {
  if(!s_window) {
    s_window = window_create();
    window_set_window_handlers(s_window, (WindowHandlers) {
      .load = window_load,
      .unload = window_unload,
    });
  }
  window_stack_push(s_window, true);
}

void loading_window_pop() {
  if(s_window) {
    window_stack_remove(s_window, true);
  }
}