#include <pebble.h>
#include "modules/app_message.h"
#include "windows/main_window.h"
#include "windows/loading_window.h"
#include "windows/join_channel_window.h"

// Track connection status and voice channel status
static bool s_is_connected = false;
static bool s_is_in_channel = false;
static AppTimer *s_leave_timer = NULL;

// Make the transition to join channel more robust
static void delayed_transition_to_join(void *data) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Executing delayed transition to join window");
  
  // Clear the timer pointer
  s_leave_timer = NULL;
  
  // Force transition regardless of channel state
  if (s_is_connected) {
    if (window_stack_contains_window(main_window_get_window())) {
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Removing main window from stack");
      main_window_pop();
    }
    
    if (!window_stack_contains_window(join_channel_window_get_window())) {
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Adding join window to stack");
      join_channel_window_push();
    }
  }
}

// Update check_channel_status to force sync the state
static void check_channel_status(void *data) {
  bool in_channel_window = window_stack_contains_window(main_window_get_window());
  bool in_join_window = window_stack_contains_window(join_channel_window_get_window());
  
  // Force sync with main_window state
  bool window_reports_in_channel = main_window_is_in_channel();
  if (window_reports_in_channel != s_is_in_channel) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "State inconsistency: main.c thinks in_channel=%d, main_window.c thinks in_channel=%d", 
            s_is_in_channel, window_reports_in_channel);
    s_is_in_channel = window_reports_in_channel;
  }
  
  // If state is inconsistent, fix it
  if (s_is_connected) {
    if (s_is_in_channel && !in_channel_window) {
      // Should be in channel window but aren't
      APP_LOG(APP_LOG_LEVEL_INFO, "State inconsistency: should be in channel window");
      if (in_join_window) {
        join_channel_window_pop();
      }
      main_window_push();
    } else if (!s_is_in_channel && !in_join_window) {
      // Should be in join window or waiting to switch
      APP_LOG(APP_LOG_LEVEL_INFO, "State inconsistency: should be in join window");
      
      // If no timer is active, switch immediately
      if (s_leave_timer == NULL) {
        if (in_channel_window) {
          main_window_pop();
        }
        join_channel_window_push();
      }
    }
  }
  
  // Schedule the next check
  app_timer_register(1000, check_channel_status, NULL);
}

static void voice_info_callback(const char* channel_name, int user_count, const char* server_name) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Voice info received - Channel: '%s', Users: %d", 
          channel_name, user_count);
  
  // Determine if we're in a voice channel - proper channel name and user count > 0
  bool is_in_channel = (strcmp(channel_name, "") != 0 && 
                       strcmp(channel_name, "Loading...") != 0 && 
                       user_count > 0);
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Voice info: in_channel=%d, current_state=%d", is_in_channel, s_is_in_channel);
  
  // Handle LEAVING a channel - more explicit detection
  if (s_is_in_channel && !is_in_channel) {
    APP_LOG(APP_LOG_LEVEL_INFO, "LEFT CHANNEL: Scheduling transition to join window");
    
    // IMPORTANT: Update global state variable immediately
    s_is_in_channel = false;
    
    // Update UI immediately so it shows blank
    main_window_update_voice_info(channel_name, user_count, server_name);
    
    // Cancel any existing timer and create a new one
    if (s_leave_timer) {
      app_timer_cancel(s_leave_timer);
    }
    s_leave_timer = app_timer_register(2000, delayed_transition_to_join, NULL);
    return;
  }
  
  // Handle JOINING a channel
  if (!s_is_in_channel && is_in_channel) {
    APP_LOG(APP_LOG_LEVEL_INFO, "JOINED CHANNEL: Transitioning to main window");
    s_is_in_channel = true;
    
    // Cancel any pending leave timer
    if (s_leave_timer) {
      app_timer_cancel(s_leave_timer);
      s_leave_timer = NULL;
    }
    
    // Force window transition
    join_channel_window_pop();
    main_window_push();
    
    // Explicitly update the main window
    main_window_update_voice_info(channel_name, user_count, server_name);
    return;
  }
  
  // If no state change but we have updated data
  if (is_in_channel && s_is_connected) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Updating main window with latest voice info");
    main_window_update_voice_info(channel_name, user_count, server_name);
  }
}

// Update connection_handler to also check the main_window state
static void connection_handler(bool is_connected) {
  s_is_connected = is_connected;
  
  if (is_connected) {
    // We're connected - transition depends on channel state
    loading_window_pop();
    
    // Start monitoring channel status
    app_timer_register(1000, check_channel_status, NULL);
    
    // Get the real in-channel state from main_window
    s_is_in_channel = main_window_is_in_channel();
    
    if (s_is_in_channel) {
      join_channel_window_pop();
      main_window_push();
    } else {
      main_window_pop();
      join_channel_window_push();
    }
  } else {
    // Not connected - show loading screen
    if (s_leave_timer) {
      app_timer_cancel(s_leave_timer);
      s_leave_timer = NULL;
    }
    
    main_window_pop();
    join_channel_window_pop();
    loading_window_push();
  }
}

static void init() {
  // Initialize app message system
  init_app_message();
  
  // Register for connection updates and voice info
  register_connection_callback(connection_handler);
  register_voice_info_callback(voice_info_callback);
  
  // Start with the loading window
  loading_window_push();
}

static void deinit() {
  // Clean up any pending timers
  if (s_leave_timer) {
    app_timer_cancel(s_leave_timer);
  }
}

int main() {
  init();
  app_event_loop();
  deinit();
}