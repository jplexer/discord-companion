#include "app_message.h"
#include <string.h>

// State tracking
static bool s_is_muted = false;
static bool s_is_deafened = false;

// Callback storage
static StateChangeCallback s_state_change_callback = NULL;
static VoiceInfoCallback s_voice_info_callback = NULL;

// Voice info storage
static char s_voice_channel_name[64] = "Loading...";
static int s_voice_user_count = 0;
static char s_voice_topic[128] = "";

void register_state_change_callback(StateChangeCallback callback) {
  s_state_change_callback = callback;
}

void register_voice_info_callback(VoiceInfoCallback callback) {
  s_voice_info_callback = callback;
}

void inbox_received_callback(DictionaryIterator *iter, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Message received!");
  
  bool state_changed = false;
  bool voice_info_changed = false;
  
  // Check for mute state updates
  Tuple *mute_tuple = dict_find(iter, MESSAGE_KEY_MUTE_STATE);
  if(mute_tuple) {
    s_is_muted = (mute_tuple->value->uint8 == 1);
    state_changed = true;
  }
  
  // Check for deafen state updates
  Tuple *deafen_tuple = dict_find(iter, MESSAGE_KEY_DEAFEN_STATE);
  if(deafen_tuple) {
    s_is_deafened = (deafen_tuple->value->uint8 == 1);
    state_changed = true;
  }
  
  // Check for voice channel info updates
  Tuple *voice_name_tuple = dict_find(iter, MESSAGE_KEY_VOICE_CHANNEL_NAME);
  if(voice_name_tuple) {
    strncpy(s_voice_channel_name, voice_name_tuple->value->cstring, sizeof(s_voice_channel_name) - 1);
    s_voice_channel_name[sizeof(s_voice_channel_name) - 1] = '\0';
    voice_info_changed = true;
    APP_LOG(APP_LOG_LEVEL_INFO, "Received channel name: %s", s_voice_channel_name);
  }
  
  Tuple *user_count_tuple = dict_find(iter, MESSAGE_KEY_VOICE_USER_COUNT);
  if(user_count_tuple) {
    s_voice_user_count = user_count_tuple->value->int32;
    voice_info_changed = true;
    APP_LOG(APP_LOG_LEVEL_INFO, "Received user count: %d", s_voice_user_count);
  }
  
  Tuple *topic_tuple = dict_find(iter, MESSAGE_KEY_VOICE_TOPIC);
  if(topic_tuple) {
    strncpy(s_voice_topic, topic_tuple->value->cstring, sizeof(s_voice_topic) - 1);
    s_voice_topic[sizeof(s_voice_topic) - 1] = '\0';
    voice_info_changed = true;
    APP_LOG(APP_LOG_LEVEL_INFO, "Received topic: %s", s_voice_topic);
  }
  
  // Notify the UI if state changed and callback is registered
  if(state_changed && s_state_change_callback) {
    s_state_change_callback(s_is_muted, s_is_deafened);
  }
  
  // Notify about voice info changes
  if(voice_info_changed && s_voice_info_callback) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Calling voice info callback");
    s_voice_info_callback(s_voice_channel_name, s_voice_user_count, s_voice_topic);
  }
}
  
void inbox_dropped_callback(AppMessageResult reason, void *context) {
  // A message was received, but had to be dropped
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped. Reason: %d", (int)reason);
}

void outbox_failed_callback(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  // The message just sent failed to be delivered
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message send failed. Reason: %d", (int)reason);
}