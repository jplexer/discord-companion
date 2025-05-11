#include "main_window.h"
#include "../modules/app_message.h"
#include "../util/style.h"
#include <pebble.h>

// ---------------------- DECLARATIONS ----------------------

// Window and UI components
static Window *s_window;
static ActionBarLayer *s_action_bar;
static StatusBarLayer *s_status_bar;
static Window *s_confirm_window;
static ActionBarLayer *s_confirm_action_bar;
static TextLayer *s_confirm_text_layer;

// Remove the menu layer declaration:
// static MenuLayer *s_confirm_menu_layer;

// Add action bar icons for confirmation
static GBitmap *s_confirm_yes_icon;
static GBitmap *s_confirm_no_icon;

// Text layers
static TextLayer *s_server_name_layer;
static TextLayer *s_channel_name_layer;
static TextLayer *s_user_count_layer;

// Discord logo
static GDrawCommandImage *s_discord_icon;
static Layer *s_discord_layer;

// Icon resources
static GBitmap *s_mute_on_icon;
static GBitmap *s_mute_off_icon;
static GBitmap *s_deafen_on_icon;
static GBitmap *s_deafen_off_icon;
static GBitmap *s_leave_icon;

// State tracking
static bool s_is_muted = false;
static bool s_is_deafened = false;
static bool s_is_in_channel = false;
static bool s_is_window_loaded = false;
static bool s_has_pending_data = false;

// Voice info storage
static char s_server_name_text[64] = "";
static char s_channel_name_text[64] = "";
static char s_user_count_text[16] = "";

// Pending voice info (before window is loaded)
static char s_pending_server_name[64] = "";
static char s_pending_channel_name[64] = "";
static int s_pending_user_count = 0;

// Forward declarations
static void update_action_bar_icons(void);
static void update_layout(void);
static void show_leave_confirmation(void);
static void voice_info_handler(const char* channel_name, int user_count, const char* server_name);

// Add these declarations at the top with other declarations
static GDrawCommandImage *s_confirm_icon;
static Layer *s_confirm_icon_layer;

// ---------------------- CONFIRMATION DIALOG ----------------------

static void confirm_yes_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Send the leave channel message
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  
  if (iter == NULL) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Cannot create outbox for leave channel message");
    return;
  }
  
  dict_write_uint8(iter, MESSAGE_KEY_LEAVE_CHANNEL, 1);
  app_message_outbox_send();
  
  // Close the confirmation window
  window_stack_remove(s_confirm_window, true);
}

static void confirm_no_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Just close the confirmation window
  window_stack_remove(s_confirm_window, true);
}

static void confirm_action_bar_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_DOWN, confirm_no_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, confirm_yes_click_handler);
}

// Add this function to draw the question mark icon
static void confirm_icon_layer_update_proc(Layer *layer, GContext *ctx) {
  if (!s_confirm_icon) return;
  
  // Get the bounds of the layer
  GRect bounds = layer_get_bounds(layer);
  
  // Calculate position to center the icon in the layer
  GSize icon_size = gdraw_command_image_get_bounds_size(s_confirm_icon);
  GPoint center = GPoint(bounds.size.w / 2, bounds.size.h / 2);
  GPoint draw_point = GPoint(
    center.x - icon_size.w / 2,
    center.y - icon_size.h / 2
  );
  
  // Draw the PDC vector image centered in the layer
  gdraw_command_image_draw(ctx, s_confirm_icon, draw_point);
}

static void confirm_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  // Calculate available width (account for action bar)
  int available_width = bounds.size.w - ACTION_BAR_WIDTH;
  
  // Create question mark icon layer (centered horizontally)
  s_confirm_icon = gdraw_command_image_create_with_resource(RESOURCE_ID_QUESTION_MARK);
  #if PBL_COLOR
    s_confirm_icon_layer = layer_create(GRect(15, 20, available_width, 80));
  #else
    s_confirm_icon_layer = layer_create(GRect(0, 10, available_width, 80));
  #endif
  layer_set_update_proc(s_confirm_icon_layer, confirm_icon_layer_update_proc);
  layer_add_child(window_layer, s_confirm_icon_layer);
  
  // Create text layer for confirmation message (below the icon)
  s_confirm_text_layer = text_layer_create(GRect(10, 100, available_width - 20, 50));
  text_layer_set_text(s_confirm_text_layer, "Leave voice channel?");
  text_layer_set_text_alignment(s_confirm_text_layer, GTextAlignmentCenter);
  text_layer_set_font(s_confirm_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_color(s_confirm_text_layer, FOREGROUND_COLOR);
  text_layer_set_background_color(s_confirm_text_layer, GColorClear);

  #if PBL_ROUND
    text_layer_set_text_alignment(s_confirm_text_layer, GTextAlignmentRight);
  #endif
  
  layer_add_child(window_layer, text_layer_get_layer(s_confirm_text_layer));
  
  // Create action bar
  s_confirm_action_bar = action_bar_layer_create();
  action_bar_layer_add_to_window(s_confirm_action_bar, window);
  
  // Load icons
  s_confirm_yes_icon = gbitmap_create_with_resource(RESOURCE_ID_CONFIRM_ICON);
  s_confirm_no_icon = gbitmap_create_with_resource(RESOURCE_ID_DISMISS_ICON);
  
  // Set action bar icons
  action_bar_layer_set_icon(s_confirm_action_bar, BUTTON_ID_UP, s_confirm_yes_icon);
  action_bar_layer_set_icon(s_confirm_action_bar, BUTTON_ID_DOWN, s_confirm_no_icon);
  
  // Set click config provider
  action_bar_layer_set_click_config_provider(s_confirm_action_bar, confirm_action_bar_click_config_provider);
  
  #if PBL_COLOR
    action_bar_layer_set_background_color(s_confirm_action_bar, GColorBlack);
  #endif
}

static void confirm_window_unload(Window *window) {
  // Destroy PDC resources
  if (s_confirm_icon_layer) layer_destroy(s_confirm_icon_layer);
  if (s_confirm_icon) gdraw_command_image_destroy(s_confirm_icon);
  
  // Destroy text and action bar
  text_layer_destroy(s_confirm_text_layer);
  action_bar_layer_destroy(s_confirm_action_bar);
  
  if (s_confirm_yes_icon) gbitmap_destroy(s_confirm_yes_icon);
  if (s_confirm_no_icon) gbitmap_destroy(s_confirm_no_icon);
  
  window_destroy(s_confirm_window);
  s_confirm_window = NULL;
}

static void show_leave_confirmation(void) {
  s_confirm_window = window_create();
  window_set_window_handlers(s_confirm_window, (WindowHandlers) {
    .load = confirm_window_load,
    .unload = confirm_window_unload
  });
  
  #if PBL_COLOR
    window_set_background_color(s_confirm_window, GColorIndigo);
  #endif
  
  window_stack_push(s_confirm_window, true);
}

// ---------------------- APP GLANCE ----------------------

static void prv_update_app_glance(AppGlanceReloadSession *session, size_t limit, void *context) {
  return; // Disable AppGlance for now
  const AppGlanceSlice slice = {
    .layout = {
      .icon = APP_GLANCE_SLICE_DEFAULT_ICON,
      .subtitle_template_string = s_channel_name_text,
    },
    .expiration_time = time(NULL) + 30 * 60 // Expire after 30 minutes
  };
  
  AppGlanceResult result = app_glance_add_slice(session, slice);
  if (result != APP_GLANCE_RESULT_SUCCESS) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to add app glance: %d", result);
  }
}

// ---------------------- UI LAYOUT FUNCTIONS ----------------------

static void update_layout(void) {
  if (!s_channel_name_layer || !s_user_count_layer) {
    return;
  }
  
  // Get the actual height of the channel name text
  GSize channel_text_size = text_layer_get_content_size(s_channel_name_layer);
  GRect channel_frame = layer_get_frame(text_layer_get_layer(s_channel_name_layer));
  
  // Calculate the position for the user count layer (with 5px padding)
  int user_count_y = channel_frame.origin.y + channel_text_size.h + 5;
  
  // Update the user count layer position
  GRect user_count_frame = layer_get_frame(text_layer_get_layer(s_user_count_layer));
  user_count_frame.origin.y = user_count_y;
  layer_set_frame(text_layer_get_layer(s_user_count_layer), user_count_frame);
}

static void discord_layer_update_proc(Layer *layer, GContext *ctx) {
  if (!s_discord_icon) return;
  
  // Draw the PDC vector image at (0,0) in the layer's coordinate system
  gdraw_command_image_draw(ctx, s_discord_icon, GPoint(0, 0));
}

// ---------------------- DATA HANDLERS ----------------------

static void voice_info_handler(const char* channel_name, int user_count, const char* server_name) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Main window received voice info - Channel: '%s', Users: %d", 
          channel_name, user_count);

  // Determine if we're in a channel
  s_is_in_channel = (strcmp(channel_name, "") != 0 && user_count > 0);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Channel status updated: is_in_channel=%d", s_is_in_channel);
  
  // If window isn't fully loaded yet, store data for later
  if (!s_is_window_loaded) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Window not loaded yet, storing voice info for later");
    strncpy(s_pending_channel_name, channel_name, sizeof(s_pending_channel_name) - 1);
    s_pending_channel_name[sizeof(s_pending_channel_name) - 1] = '\0';
    s_pending_user_count = user_count;
    strncpy(s_pending_server_name, server_name, sizeof(s_pending_server_name) - 1);
    s_pending_server_name[sizeof(s_pending_server_name) - 1] = '\0';
    s_has_pending_data = true;
    return;
  }

  // Process server name
  if (strcmp(server_name, "") == 0) {
    s_server_name_text[0] = '\0';
  } else {
    strncpy(s_server_name_text, server_name, sizeof(s_server_name_text) - 1);
    s_server_name_text[sizeof(s_server_name_text) - 1] = '\0';
  }

  // Process channel information
  if (strcmp(channel_name, "") == 0 || user_count == 0) {
    s_channel_name_text[0] = '\0';
    s_user_count_text[0] = '\0';
  } else {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Updating UI with channel=%s, users=%d", channel_name, user_count);
    
    // Set channel name
    strncpy(s_channel_name_text, channel_name, sizeof(s_channel_name_text) - 1);
    s_channel_name_text[sizeof(s_channel_name_text) - 1] = '\0';
    
    // Set user count
    if (user_count <= 1) {
      strncpy(s_user_count_text, "Just you", sizeof(s_user_count_text));
    } else if (user_count == 2) {
      snprintf(s_user_count_text, sizeof(s_user_count_text), "with 1 other");
    } else {
      snprintf(s_user_count_text, sizeof(s_user_count_text), "with %d others", user_count - 1);
    }
  }
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Setting text layers: channel='%s', users='%s'", 
         s_channel_name_text, s_user_count_text);
  
  // Update text layers
  text_layer_set_text(s_server_name_layer, s_server_name_text);
  text_layer_set_text(s_channel_name_layer, s_channel_name_text);
  text_layer_set_text(s_user_count_layer, s_user_count_text);
  
  // Update layout to position user count correctly
  update_layout();
}

static void update_discord_icon(void) {
  // Free the existing icon if it exists
  if (s_discord_icon) {
    gdraw_command_image_destroy(s_discord_icon);
  }
  
  // Choose the appropriate icon based on status
  if (s_is_deafened) {
    s_discord_icon = gdraw_command_image_create_with_resource(RESOURCE_ID_STATUS_DEAFENED);
  } else if (s_is_muted) {
    s_discord_icon = gdraw_command_image_create_with_resource(RESOURCE_ID_STATUS_MUTED);
  } else {
    s_discord_icon = gdraw_command_image_create_with_resource(RESOURCE_ID_DISCORD_50);
  }
  
  // Request a redraw of the Discord layer
  if (s_discord_layer) {
    layer_mark_dirty(s_discord_layer);
  }
}

static void state_change_handler(bool is_muted, bool is_deafened) {
  s_is_muted = is_muted;
  s_is_deafened = is_deafened;
  
  update_action_bar_icons();
  update_discord_icon(); // Update the Discord icon when status changes
}

// ---------------------- BUTTON ACTIONS ----------------------

static void mute_click_handler(ClickRecognizerRef recognizer, void *context) {
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  
  if (iter == NULL) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Cannot create outbox for toggle mute message");
    return;
  }
  
  dict_write_uint8(iter, MESSAGE_KEY_TOGGLE_MUTE, 1);
  app_message_outbox_send();
}

static void deafen_click_handler(ClickRecognizerRef recognizer, void *context) {
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  
  if (iter == NULL) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Cannot create outbox for toggle deafen message");
    return;
  }
  
  dict_write_uint8(iter, MESSAGE_KEY_TOGGLE_DEAFEN, 1);
  app_message_outbox_send();
}

static void leave_click_handler(ClickRecognizerRef recognizer, void *context) {
  show_leave_confirmation();
}

static void action_bar_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, deafen_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, mute_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, leave_click_handler);
}

static void update_action_bar_icons(void) {
  if (!s_action_bar) return;
  
  action_bar_layer_set_background_color(s_action_bar, GColorBlack);
  action_bar_layer_set_icon(s_action_bar, BUTTON_ID_SELECT, s_leave_icon);
  action_bar_layer_set_icon(s_action_bar, BUTTON_ID_DOWN, 
                          s_is_muted ? s_mute_on_icon : s_mute_off_icon);
  action_bar_layer_set_icon(s_action_bar, BUTTON_ID_UP, 
                          s_is_deafened ? s_deafen_on_icon : s_deafen_off_icon);
}

// ---------------------- WINDOW LIFECYCLE ----------------------

static void create_text_layers(Layer *window_layer, GRect bounds, int status_bar_height) {
  // Calculate available width and position based on screen shape
  int available_width, x_offset, y_offset;
  GTextAlignment text_alignment;
  
  #if PBL_ROUND
    available_width = bounds.size.w - ACTION_BAR_WIDTH - 24;
    x_offset = 12;
    y_offset = status_bar_height + 10;
    text_alignment = GTextAlignmentRight;
  #else
    available_width = bounds.size.w - ACTION_BAR_WIDTH - 8;
    x_offset = 4;
    y_offset = status_bar_height + 5;
    text_alignment = GTextAlignmentLeft;
  #endif
  
  // 1. Server name layer
  s_server_name_layer = text_layer_create(GRect(x_offset, y_offset, available_width, 30));
  text_layer_set_text_alignment(s_server_name_layer, text_alignment);
  text_layer_set_font(s_server_name_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text(s_server_name_layer, "");
  text_layer_set_overflow_mode(s_server_name_layer, GTextOverflowModeFill);
  text_layer_set_text_color(s_server_name_layer, FOREGROUND_COLOR);
  text_layer_set_background_color(s_server_name_layer, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(s_server_name_layer));
  
  // 2. Channel name layer
  y_offset += 20;
  #if PBL_ROUND
    s_channel_name_layer = text_layer_create(GRect(x_offset, y_offset, available_width, 70));
  #else
    s_channel_name_layer = text_layer_create(GRect(x_offset, y_offset, available_width, 85));
  #endif
  
  text_layer_set_text_alignment(s_channel_name_layer, text_alignment);
  text_layer_set_font(s_channel_name_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text(s_channel_name_layer, "");
  text_layer_set_overflow_mode(s_channel_name_layer, GTextOverflowModeTrailingEllipsis);
  text_layer_set_size(s_channel_name_layer, GSize(available_width, 60));
  text_layer_set_text_color(s_channel_name_layer, FOREGROUND_COLOR);
  text_layer_set_background_color(s_channel_name_layer, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(s_channel_name_layer));
  
  // 3. User count layer
  #if PBL_ROUND
    y_offset += 80;
  #else
    y_offset += 95;
  #endif
  s_user_count_layer = text_layer_create(GRect(x_offset, y_offset, available_width, 30));
  text_layer_set_text_alignment(s_user_count_layer, text_alignment);
  text_layer_set_font(s_user_count_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text(s_user_count_layer, "");
  text_layer_set_text_color(s_user_count_layer, FOREGROUND_COLOR);
  text_layer_set_background_color(s_user_count_layer, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(s_user_count_layer));
}

static void create_action_bar(Window *window) {
  s_action_bar = action_bar_layer_create();
  action_bar_layer_add_to_window(s_action_bar, window);
  action_bar_layer_set_click_config_provider(s_action_bar, action_bar_click_config_provider);
  
  // Load icons
  s_mute_off_icon = gbitmap_create_with_resource(RESOURCE_ID_MUTE_OFF_ICON);
  s_mute_on_icon = gbitmap_create_with_resource(RESOURCE_ID_MUTE_ON_ICON);
  s_deafen_off_icon = gbitmap_create_with_resource(RESOURCE_ID_DEAFEN_OFF_ICON);
  s_deafen_on_icon = gbitmap_create_with_resource(RESOURCE_ID_DEAFEN_ON_ICON);
  s_leave_icon = gbitmap_create_with_resource(RESOURCE_ID_DISMISS_ICON);
  
  update_action_bar_icons();
}

static void create_discord_logo(Layer *window_layer, GRect bounds) {
  s_discord_icon = gdraw_command_image_create_with_resource(RESOURCE_ID_DISCORD_50);
  
  GRect discord_frame;
  #if PBL_ROUND
    discord_frame = GRect(bounds.size.w - ACTION_BAR_WIDTH - 14 - 50, bounds.size.h - 50 - 4, 50, 50);
  #else
    discord_frame = GRect(8, bounds.size.h - 53, 50, 50);
  #endif
  
  s_discord_layer = layer_create(discord_frame);
  layer_set_update_proc(s_discord_layer, discord_layer_update_proc);
  layer_add_child(window_layer, s_discord_layer);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  window_set_background_color(window, BACKGROUND_COLOR);
  
  // Create status bar
  s_status_bar = status_bar_layer_create();
  status_bar_layer_set_colors(s_status_bar, BACKGROUND_COLOR, FOREGROUND_COLOR);
  status_bar_layer_set_separator_mode(s_status_bar, StatusBarLayerSeparatorModeNone);
  #if defined(PBL_RECT)
    // Move statusbar over to make room for action bar
    GRect status_bar_frame = GRect(0, 0, bounds.size.w - ACTION_BAR_WIDTH, STATUS_BAR_LAYER_HEIGHT);
    layer_set_frame(status_bar_layer_get_layer(s_status_bar), status_bar_frame);
  #endif
  layer_add_child(window_layer, status_bar_layer_get_layer(s_status_bar));
  
  // Get status bar height
  GRect status_bar_bounds = layer_get_bounds(status_bar_layer_get_layer(s_status_bar));
  const int status_bar_height = status_bar_bounds.size.h;
  
  // Create text layers
  create_text_layers(window_layer, bounds, status_bar_height);
  
  // Create action bar
  create_action_bar(window);
  
  // Create Discord logo
  create_discord_logo(window_layer, bounds);

  // Set flag indicating window is now loaded
  s_is_window_loaded = true;
  APP_LOG(APP_LOG_LEVEL_INFO, "Main window load complete");
  
  // Apply any pending voice info
  if (s_has_pending_data) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Applying pending voice info data: %s, %d", 
           s_pending_channel_name, s_pending_user_count);
    voice_info_handler(s_pending_channel_name, s_pending_user_count, s_pending_server_name);
    s_has_pending_data = false;
  }
}

static void window_unload(Window *window) {
  // Destroy UI elements
  if (s_status_bar) status_bar_layer_destroy(s_status_bar);
  if (s_channel_name_layer) text_layer_destroy(s_channel_name_layer);
  if (s_user_count_layer) text_layer_destroy(s_user_count_layer);
  if (s_server_name_layer) text_layer_destroy(s_server_name_layer);
  if (s_discord_layer) layer_destroy(s_discord_layer);
  if (s_discord_icon) gdraw_command_image_destroy(s_discord_icon);
  if (s_action_bar) action_bar_layer_destroy(s_action_bar);
  
  // Destroy bitmaps
  if (s_mute_off_icon) gbitmap_destroy(s_mute_off_icon);
  if (s_mute_on_icon) gbitmap_destroy(s_mute_on_icon);
  if (s_deafen_off_icon) gbitmap_destroy(s_deafen_off_icon);
  if (s_deafen_on_icon) gbitmap_destroy(s_deafen_on_icon);
  if (s_leave_icon) gbitmap_destroy(s_leave_icon);
  
  app_glance_reload(prv_update_app_glance, NULL);
  window_destroy(s_window);
  s_window = NULL;
  s_is_window_loaded = false;
}

// ---------------------- PUBLIC FUNCTIONS ----------------------

void main_window_update_voice_info(const char* channel_name, int user_count, const char* server_name) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "External update to main window: Channel='%s', Users=%d", 
          channel_name, user_count);
          
  // If window isn't loaded yet, store info for later
  if (!s_is_window_loaded) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Window not loaded, storing for later");
    strncpy(s_pending_channel_name, channel_name, sizeof(s_pending_channel_name) - 1);
    s_pending_channel_name[sizeof(s_pending_channel_name) - 1] = '\0';
    strncpy(s_pending_server_name, server_name, sizeof(s_pending_server_name) - 1);
    s_pending_server_name[sizeof(s_pending_server_name) - 1] = '\0';
    s_pending_user_count = user_count;
    s_has_pending_data = true;
    return;
  }
  
  voice_info_handler(channel_name, user_count, server_name);
}

void main_window_push() {
  if (!s_window) {
    register_state_change_callback(state_change_handler);
    register_voice_info_callback(voice_info_handler);
    
    s_window = window_create();
    window_set_window_handlers(s_window, (WindowHandlers) {
      .load = window_load,
      .unload = window_unload,
    });
  }
  window_stack_push(s_window, true);
}

void main_window_pop() {
  if (s_window) {
    window_stack_remove(s_window, false);
  }
}

Window* main_window_get_window() {
  return s_window;
}

bool main_window_is_in_channel() {
  return s_is_in_channel;
}