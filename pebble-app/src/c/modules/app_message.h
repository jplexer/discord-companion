#pragma once

#include <pebble.h>

// Callback types
typedef void (*StateChangeCallback)(bool is_muted, bool is_deafened);
typedef void (*VoiceInfoCallback)(const char* channel_name, int user_count, const char* topic);
typedef void (*ConnectionCallback)(bool is_connected);

void register_state_change_callback(StateChangeCallback callback);
void register_voice_info_callback(VoiceInfoCallback callback);
void register_connection_callback(ConnectionCallback callback);

void inbox_received_callback(DictionaryIterator *iterator, void *context);
void inbox_dropped_callback(AppMessageResult reason, void *context);
void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context);

void init_app_message(void);