#include <pebble.h>

static Window *s_window;
static Layer *s_window_layer;
static TextLayer *s_time_layer;

static char s_current_time_buffer[8];

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  strftime(s_current_time_buffer, sizeof(s_current_time_buffer),
           clock_is_24h_style() ? "%H:%M" : "%l:%M", tick_time);

  text_layer_set_text(s_time_layer, s_current_time_buffer);
}

static void window_load(Window *window) {
  GRect window_bounds = layer_get_bounds(s_window_layer);

  // Create a layer to hold the current time
  s_time_layer = text_layer_create(
      GRect(0, PBL_IF_ROUND_ELSE(82, 78), window_bounds.size.w, 38));
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_font(s_time_layer,
                      fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(s_window_layer, text_layer_get_layer(s_time_layer));
}

static void window_unload(Window *window) {
  layer_destroy(text_layer_get_layer(s_time_layer));
  layer_destroy(s_window_layer);
  window_destroy(s_window);
}

void init() {
  s_window = window_create();
  s_window_layer = window_get_root_layer(s_window);
  window_set_background_color(s_window, GColorBlack);

  window_set_window_handlers(s_window,
                             (WindowHandlers) {
                                 .load = window_load,
                                 .unload = window_unload
                             });

  window_stack_push(s_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

void deinit() {

}

int main() {
  init();
  app_event_loop();
  deinit();
}
