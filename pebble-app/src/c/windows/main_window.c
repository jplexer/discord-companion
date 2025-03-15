#include "main_window.h"
#include "../modules/app_message.h"
#include <pebble.h>

static Window *s_window;
static ActionBarLayer *s_action_bar;
static StatusBarLayer *s_status_bar;

// State tracking
static bool s_is_muted = false;
static bool s_is_deafened = false;
static bool s_is_in_channel = false;

// Voice info text layers
static TextLayer *s_channel_name_layer;
static TextLayer *s_user_count_layer;

// Voice info storage
static char s_channel_name_text[64] = "";
static char s_user_count_text[16] = "";

// Bitmap resources for icons - we need both on/off states
static GBitmap *s_mute_on_icon;
static GBitmap *s_mute_off_icon;
static GBitmap *s_deafen_on_icon;
static GBitmap *s_deafen_off_icon;
static GBitmap *s_leave_icon;

static GDrawCommandImage *s_discord_icon;
static Layer *s_discord_layer;

static bool s_is_window_loaded = false;
static char s_pending_channel_name[64] = "";
static int s_pending_user_count = 0;
static bool s_has_pending_data = false;

// Forward declare
static void update_action_bar_icons(void);

// Confirmation menu components
static MenuLayer *s_confirm_menu_layer;
static Window *s_confirm_window;

static uint16_t get_num_confirm_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return 2; // Yes and No options
}

static void draw_confirm_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  switch (cell_index->row) {
    case 0:
      menu_cell_basic_draw(ctx, cell_layer, "No", "Stay in channel", NULL);
      break;
    case 1:
      menu_cell_basic_draw(ctx, cell_layer, "Yes", "Leave channel", NULL);
      break;
  }
}

static void confirm_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  if (cell_index->row == 1) { // User confirmed "Yes"
    // Send the leave channel message
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);
    
    if (iter == NULL) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Cannot create outbox for leave channel message");
      return;
    }
    
    dict_write_uint8(iter, MESSAGE_KEY_LEAVE_CHANNEL, 1);
    app_message_outbox_send();
  }
  
  // Close the confirmation window in either case
  window_stack_remove(s_confirm_window, true);
}

static void confirm_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  // Create menu layer
  s_confirm_menu_layer = menu_layer_create(bounds);
  
  // Set callbacks
  menu_layer_set_callbacks(s_confirm_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = get_num_confirm_rows,
    .draw_row = draw_confirm_row,
    .select_click = confirm_select_callback
  });
  
  // Add to window
  menu_layer_set_click_config_onto_window(s_confirm_menu_layer, window);
  layer_add_child(window_layer, menu_layer_get_layer(s_confirm_menu_layer));
}

static void confirm_window_unload(Window *window) {
  menu_layer_destroy(s_confirm_menu_layer);
  window_destroy(s_confirm_window);
  s_confirm_window = NULL;
}

static void show_leave_confirmation() {
  // Create confirmation window
  s_confirm_window = window_create();
  window_set_window_handlers(s_confirm_window, (WindowHandlers) {
    .load = confirm_window_load,
    .unload = confirm_window_unload
  });
  
  #ifdef PBL_COLOR
    window_set_background_color(s_confirm_window, GColorLiberty);
  #endif
  
  window_stack_push(s_confirm_window, true);
}

static void prv_update_app_glance(AppGlanceReloadSession *session, size_t limit, void *context) {
  // Create glance with current timestamp
  const AppGlanceSlice slice = {
    .layout = {
      .icon = APP_GLANCE_SLICE_DEFAULT_ICON,
      .subtitle_template_string = s_channel_name_text,
    },
    //Expire after 30 minutes
    .expiration_time = time(NULL) + 30 * 60
  };
  
  // Add the app glance and log any errors
  AppGlanceResult result = app_glance_add_slice(session, slice);
  if (result != APP_GLANCE_RESULT_SUCCESS) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to add app glance: %d", result);
  }
}

static void update_layout() {
  if (!s_channel_name_layer || !s_user_count_layer) {
    return;
  }
  
  // Get the actual height of the channel name text
  GSize channel_text_size = text_layer_get_content_size(s_channel_name_layer);
  GRect channel_frame = layer_get_frame(text_layer_get_layer(s_channel_name_layer));
  
  // Calculate the position for the user count layer
  // Add a small padding (5px) between the texts
  int user_count_y = channel_frame.origin.y + channel_text_size.h + 5;
  
  // Update the user count layer position
  GRect user_count_frame = layer_get_frame(text_layer_get_layer(s_user_count_layer));
  user_count_frame.origin.y = user_count_y;
  layer_set_frame(text_layer_get_layer(s_user_count_layer), user_count_frame);
}

// Update the voice_info_handler to log more details
static void voice_info_handler(const char* channel_name, int user_count, const char* topic) {
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
    s_has_pending_data = true;
    return;
  }

  // Update channel name layer
  if (strcmp(channel_name, "") == 0 || user_count == 0) {
    strncpy(s_channel_name_text, "", sizeof(s_channel_name_text) - 1);
    s_channel_name_text[sizeof(s_channel_name_text) - 1] = '\0';
    
    // Clear user count if not in a voice channel
    s_user_count_text[0] = '\0';
  } else {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Updating UI with channel=%s, users=%d", channel_name, user_count);
    
    // Set channel name
    strncpy(s_channel_name_text, channel_name, sizeof(s_channel_name_text) - 1);
    s_channel_name_text[sizeof(s_channel_name_text) - 1] = '\0';
    
    // Set user count
    if (user_count <= 1) {
      strncpy(s_user_count_text, "Just You", sizeof(s_user_count_text));
    } else if (user_count == 2) {
      snprintf(s_user_count_text, sizeof(s_user_count_text), 
           "with 1 other");
    } else {
      snprintf(s_user_count_text, sizeof(s_user_count_text), 
           "with %d others", user_count - 1);
    }
  }
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Setting text layers: channel='%s', users='%s'", 
         s_channel_name_text, s_user_count_text);
  
  // Update text layers
  text_layer_set_text(s_channel_name_layer, s_channel_name_text);
  text_layer_set_text(s_user_count_layer, s_user_count_text);
  
  // Update layout to position user count correctly
  update_layout();
}

// State change callback from AppMessage module
static void state_change_handler(bool is_muted, bool is_deafened) {
  s_is_muted = is_muted;
  s_is_deafened = is_deafened;
  
  // Update action bar icons
  update_action_bar_icons();
}

// Handler for middle button click (Mute)
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

static void leave_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Show confirmation dialog instead of immediately leaving
  show_leave_confirmation();
}

// Click configuration for the action bar
static void action_bar_click_config_provider(void *context) {
  // Register the button handlers
  window_single_click_subscribe(BUTTON_ID_UP, deafen_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, mute_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, leave_click_handler);
}

// Update the action bar icons based on current state
static void update_action_bar_icons() {
  if(s_action_bar) {
    // Set action bar background color for better visibility
    action_bar_layer_set_background_color(s_action_bar, GColorBlack);

    action_bar_layer_set_icon(s_action_bar, BUTTON_ID_SELECT, s_leave_icon);
    
    // Set proper icons based on states
    action_bar_layer_set_icon(s_action_bar, BUTTON_ID_DOWN, 
                              s_is_muted ? s_mute_on_icon : s_mute_off_icon);
    
    action_bar_layer_set_icon(s_action_bar, BUTTON_ID_UP, 
                              s_is_deafened ? s_deafen_on_icon : s_deafen_off_icon);
  }
}

static void discord_layer_update_proc(Layer *layer, GContext *ctx) {
  if (!s_discord_icon) return;
  
  // Draw the PDC vector image at (0,0) in the layer's coordinate system
  gdraw_command_image_draw(ctx, s_discord_icon, GPoint(0, 0));
}

// Fix the window_load function to ensure UI elements are properly initialized
static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  #ifdef PBL_COLOR
    window_set_background_color(window, GColorLiberty);
  #endif
  
  // Create status bar
  s_status_bar = status_bar_layer_create();
  #ifdef PBL_COLOR
    status_bar_layer_set_colors(s_status_bar, GColorLiberty, GColorWhite);
  #else
    status_bar_layer_set_colors(s_status_bar, GColorWhite, GColorBlack);
  #endif
  status_bar_layer_set_separator_mode(s_status_bar, StatusBarLayerSeparatorModeNone);
  layer_add_child(window_layer, status_bar_layer_get_layer(s_status_bar));
  
  // Get status bar height to adjust other elements
  GRect status_bar_bounds = layer_get_bounds(status_bar_layer_get_layer(s_status_bar));
  const int status_bar_height = status_bar_bounds.size.h;
  
  // Calculate available width (accounting for action bar)
  int available_width, x_offset;
  GTextAlignment text_alignment;
  
  // Adjust layout for round vs rectangular display
  #ifdef PBL_ROUND
    // For round displays, we need to account for the curved edges
    // Use less width and center the text
    available_width = bounds.size.w - ACTION_BAR_WIDTH - 24; // More padding for round display
    x_offset = 12; // Offset from left edge for better appearance
    text_alignment = GTextAlignmentRight;
  #else
    // For rectangular displays, keep original layout
    available_width = bounds.size.w - ACTION_BAR_WIDTH - 8; // 4px padding on each side
    x_offset = 4;
    text_alignment = GTextAlignmentLeft;
  #endif
  
  // Create text layers for voice channel info
  // 1. Channel name (top) - Using more vertical space now that we don't have topic
  // Adjust y position to account for status bar
  #ifdef PBL_ROUND
    s_channel_name_layer = text_layer_create(GRect(x_offset, status_bar_height + 10, available_width, 70));
  #else
    s_channel_name_layer = text_layer_create(GRect(x_offset, status_bar_height + 5, available_width, 85));
  #endif
  
  text_layer_set_text_alignment(s_channel_name_layer, text_alignment);
  text_layer_set_font(s_channel_name_layer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
  text_layer_set_text(s_channel_name_layer, "");
  text_layer_set_overflow_mode(s_channel_name_layer, GTextOverflowModeTrailingEllipsis);

  #ifdef PBL_COLOR
    text_layer_set_text_color(s_channel_name_layer, GColorWhite);
    text_layer_set_background_color(s_channel_name_layer, GColorClear);
  #endif

  GSize max_size = GSize(available_width, 85); // Maximum height to prevent overflow
  text_layer_set_size(s_channel_name_layer, max_size);
  layer_add_child(window_layer, text_layer_get_layer(s_channel_name_layer));
  
  // 2. User count (bottom) - Initial position will be updated dynamically
  // Initial position adjusted for status bar
  #ifdef PBL_ROUND
    s_user_count_layer = text_layer_create(GRect(x_offset, status_bar_height + 90, available_width, 30));
  #else
    s_user_count_layer = text_layer_create(GRect(x_offset, status_bar_height + 100, available_width, 30));
  #endif
  
  text_layer_set_text_alignment(s_user_count_layer, text_alignment);
  text_layer_set_font(s_user_count_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text(s_user_count_layer, "");

  #ifdef PBL_COLOR
    text_layer_set_text_color(s_user_count_layer, GColorWhite);
    text_layer_set_background_color(s_user_count_layer, GColorClear);
  #endif

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
  s_leave_icon = gbitmap_create_with_resource(RESOURCE_ID_DISMISS_ICON);
  
  // Set action bar compositing mode to improve icon display
  action_bar_layer_set_background_color(s_action_bar, GColorBlack);
  
  // Set the initial icons based on state
  update_action_bar_icons();

  // Load and display Discord logo in bottom corner
  s_discord_icon = gdraw_command_image_create_with_resource(RESOURCE_ID_DISCORD_50);
  
  // Position differently based on watch shape
  GRect discord_frame;
  #ifdef PBL_ROUND
    // Bottom right for round watches - adjust position to ensure visibility
    discord_frame = GRect(bounds.size.w - 60, bounds.size.h - 60, 50, 50);
  #else
    // Bottom left for rectangular watches - move slightly away from edge
    discord_frame = GRect(8, bounds.size.h - 50, 50, 50);
  #endif
  
  // Create a custom layer instead of a bitmap layer
  s_discord_layer = layer_create(discord_frame);
  layer_set_update_proc(s_discord_layer, discord_layer_update_proc);
  
  // Add layer AFTER other elements to ensure it's on top
  layer_add_child(window_layer, s_discord_layer);

  // Set flag indicating window is now loaded
  s_is_window_loaded = true;
  APP_LOG(APP_LOG_LEVEL_INFO, "Main window load complete");
  
  // Apply any pending voice info that came in before window was ready
  if (s_has_pending_data) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Applying pending voice info data: %s, %d", 
           s_pending_channel_name, s_pending_user_count);
    voice_info_handler(s_pending_channel_name, s_pending_user_count, "");
    s_has_pending_data = false;
  }
}

static void window_unload(Window *window) {
  if (s_status_bar) {
    status_bar_layer_destroy(s_status_bar);
  }
  
  // Free voice info text layers
  if (s_channel_name_layer) {
    text_layer_destroy(s_channel_name_layer);
  }
  
  if (s_user_count_layer) {
    text_layer_destroy(s_user_count_layer);
  }
  
  // Clean up Discord icon resources
  if (s_discord_layer) {
    layer_destroy(s_discord_layer);
  }
  
  if (s_discord_icon) {
    gdraw_command_image_destroy(s_discord_icon);
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

  if (s_leave_icon) {
    gbitmap_destroy(s_leave_icon);
  }
  app_glance_reload(prv_update_app_glance, NULL);
  window_destroy(s_window);
  s_window = NULL;

  // Reset window loaded flag
  s_is_window_loaded = false;
}

// Add this function near the end of the file
void main_window_pop() {
  if (s_window) {
    window_stack_remove(s_window, false);
  }
}

Window* main_window_get_window() {
  return s_window;
}

void main_window_update_voice_info(const char* channel_name, int user_count, const char* topic) {
  // This function can be called even when the main window isn't visible
  APP_LOG(APP_LOG_LEVEL_DEBUG, "External update to main window: Channel='%s', Users=%d", 
          channel_name, user_count);
          
  // If window isn't loaded yet, store info for later
  if (!s_is_window_loaded) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Window not loaded, storing for later");
    strncpy(s_pending_channel_name, channel_name, sizeof(s_pending_channel_name) - 1);
    s_pending_channel_name[sizeof(s_pending_channel_name) - 1] = '\0';
    s_pending_user_count = user_count;
    s_has_pending_data = true;
    return;
  }
  
  // Call the regular handler to update UI
  voice_info_handler(channel_name, user_count, topic);
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

// Add this function at the end of the file
bool main_window_is_in_channel() {
  return s_is_in_channel;
}