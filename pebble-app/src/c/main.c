#include <pebble.h>

#include "modules/app_message.h"
#include "windows/main_window.h"

static void init() {
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_open(256, 256);
  
  main_window_push();
}

static void deinit() {
}

int main() {
  init();
  app_event_loop();
  deinit();
}