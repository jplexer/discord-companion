#include "main_window.h"
#include "../modules/app_message.h"
#include <pebble.h>

static Window *s_window;
static ActionBarLayer *s_action_bar;

// State tracking
static bool s_is_muted = false;
static bool s_is_deafened = false;

// Voice info text layers
static TextLayer *s_channel_name_layer;
static TextLayer *s_user_count_layer;

// Voice info storage
static char s_channel_name_text[64] = "Loading...";
static char s_user_count_text[16] = "";

// Bitmap resources for icons - we need both on/off states
static GBitmap *s_mute_on_icon;
static GBitmap *s_mute_off_icon;
static GBitmap *s_deafen_on_icon;
static GBitmap *s_deafen_off_icon;

// Forward declare
static void update_action_bar_icons(void);
static void request_voice_info(void);

// Voice info callback
static void voice_info_handler(const char* channel_name, int user_count, const char* topic) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Voice info received - Channel: %s, Users: %d", 
          channel_name, user_count);

  // Update channel name layer
  if (strcmp(channel_name, "Not in a voice channel") == 0) {
    strncpy(s_channel_name_text, channel_name, sizeof(s_channel_name_text) - 1);
    s_channel_name_text[sizeof(s_channel_name_text) - 1] = '\0';
    
    // Clear user count if not in a voice channel
    s_user_count_text[0] = '\0';
  } else {
    // Set channel name
    strncpy(s_channel_name_text, channel_name, sizeof(s_channel_name_text) - 1);
    s_channel_name_text[sizeof(s_channel_name_text) - 1] = '\0';
    
    // Set user count
    snprintf(s_user_count_text, sizeof(s_user_count_text), 
             "Users: %d", user_count);
  }
  
  // Update text layers
  text_layer_set_text(s_channel_name_layer, s_channel_name_text);
  text_layer_set_text(s_user_count_layer, s_user_count_text);
}

// State change callback from AppMessage module
static void state_change_handler(bool is_muted, bool is_deafened) {
  s_is_muted = is_muted;
  s_is_deafened = is_deafened;
  
  // Update action bar icons
  update_action_bar_icons();
}

// Handler for bottom button click (Mute)
static void mute_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Send toggleMute message to JavaScript
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  
  if (iter == NULL) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Cannot create outbox for toggle mute message");
    return;
  }
  
  // Add the toggleMute key with a dummy value
  dict_write_uint8(iter, MESSAGE_KEY_TOGGLE_MUTE, 1);
  
  // Send the message
  app_message_outbox_send();
}

// Handler for top button click (Deafen)
static void deafen_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Send toggleDeafen message to JavaScript
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  
  if (iter == NULL) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Cannot create outbox for toggle deafen message");
    return;
  }
  
  // Add the toggleDeafen key with a dummy value
  dict_write_uint8(iter, MESSAGE_KEY_TOGGLE_DEAFEN, 1);
  
  // Send the message
  app_message_outbox_send();
}

// Middle button handler for refreshing voice info
static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  //do nothing for now
}

// Click configuration for the action bar
static void action_bar_click_config_provider(void *context) {
  // Register the button handlers
  window_single_click_subscribe(BUTTON_ID_UP, deafen_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, mute_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
}

// Update the action bar icons based on current state
static void update_action_bar_icons() {
  if(s_action_bar) {
    // Set action bar background color for better visibility
    action_bar_layer_set_background_color(s_action_bar, GColorBlack);
    
    // Set proper icons based on states
    action_bar_layer_set_icon(s_action_bar, BUTTON_ID_DOWN, 
                              s_is_muted ? s_mute_on_icon : s_mute_off_icon);
    
    action_bar_layer_set_icon(s_action_bar, BUTTON_ID_UP, 
                              s_is_deafened ? s_deafen_on_icon : s_deafen_off_icon);
  }
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  // Calculate available width (accounting for action bar)
  int available_width, x_offset;
  GTextAlignment text_alignment;
  
  // Adjust layout for round vs rectangular display
  #ifdef PBL_ROUND
    // For round displays, we need to account for the curved edges
    // Use less width and center the text
    available_width = bounds.size.w - ACTION_BAR_WIDTH - 24; // More padding for round display
    x_offset = 12; // Offset from left edge for better appearance
    text_alignment = GTextAlignmentCenter;
  #else
    // For rectangular displays, keep original layout
    available_width = bounds.size.w - ACTION_BAR_WIDTH - 8; // 4px padding on each side
    x_offset = 4;
    text_alignment = GTextAlignmentLeft;
  #endif
  
  // Create text layers for voice channel info
  // 1. Channel name (top) - Using more vertical space now that we don't have topic
  #ifdef PBL_ROUND
    s_channel_name_layer = text_layer_create(GRect(x_offset, 20, available_width, 70));
  #else
    s_channel_name_layer = text_layer_create(GRect(x_offset, 15, available_width, 85));
  #endif
  
  text_layer_set_text_alignment(s_channel_name_layer, text_alignment);
  text_layer_set_font(s_channel_name_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text(s_channel_name_layer, "Loading...");
  text_layer_set_overflow_mode(s_channel_name_layer, GTextOverflowModeTrailingEllipsis);
  layer_add_child(window_layer, text_layer_get_layer(s_channel_name_layer));
  
  // 2. User count (bottom)
  #ifdef PBL_ROUND
    s_user_count_layer = text_layer_create(GRect(x_offset, 100, available_width, 30));
  #else
    s_user_count_layer = text_layer_create(GRect(x_offset, 110, available_width, 30));
  #endif
  
  text_layer_set_text_alignment(s_user_count_layer, text_alignment);
  text_layer_set_font(s_user_count_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text(s_user_count_layer, "");
  layer_add_child(window_layer, text_layer_get_layer(s_user_count_layer));
  
  // Action bar setup code
  s_action_bar = action_bar_layer_create();
  
  // Add to window
  action_bar_layer_add_to_window(s_action_bar, window);
  
  // Set click handlers
  action_bar_layer_set_click_config_provider(s_action_bar, action_bar_click_config_provider);
  
  // Load the icons with proper alignment
  s_mute_off_icon = gbitmap_create_with_resource(RESOURCE_ID_MUTE_OFF_ICON);
  s_mute_on_icon = gbitmap_create_with_resource(RESOURCE_ID_MUTE_ON_ICON);
  s_deafen_off_icon = gbitmap_create_with_resource(RESOURCE_ID_DEAFEN_OFF_ICON);
  s_deafen_on_icon = gbitmap_create_with_resource(RESOURCE_ID_DEAFEN_ON_ICON);
  
  // Set action bar compositing mode to improve icon display
  action_bar_layer_set_background_color(s_action_bar, GColorBlack);
  
  // Set the initial icons based on state
  update_action_bar_icons();
}

static void window_unload(Window *window) {
  // Free voice info text layers
  if (s_channel_name_layer) {
    text_layer_destroy(s_channel_name_layer);
  }
  
  if (s_user_count_layer) {
    text_layer_destroy(s_user_count_layer);
  }
  
  // Action bar cleanup
  if (s_action_bar) {
    action_bar_layer_destroy(s_action_bar);
  }
  
  // Destroy all bitmaps
  if (s_mute_off_icon) {
    gbitmap_destroy(s_mute_off_icon);
  }
  
  if (s_mute_on_icon) {
    gbitmap_destroy(s_mute_on_icon);
  }
  
  if (s_deafen_off_icon) {
    gbitmap_destroy(s_deafen_off_icon);
  }
  
  if (s_deafen_on_icon) {
    gbitmap_destroy(s_deafen_on_icon);
  }
  
  window_destroy(s_window);
  s_window = NULL;
}

void main_window_push() {
  if(!s_window) {
    // Register for callbacks
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