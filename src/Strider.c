#include <pebble.h>

static Window *s_window;
static Layer *s_window_layer, *s_dots_layer, *s_progress_layer, *s_average_layer;
static TextLayer *s_time_layer, *s_step_layer;

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_DIORITE)
static TextLayer *s_hrm_layer;
static int s_hr;
static char s_current_hr_buffer[8];
#endif

static char s_current_time_buffer[8], s_current_steps_buffer[16];
static int s_step_count = 0, s_step_goal = 0, s_step_average = 0;

GColor color_loser;
GColor color_winner;

void unobstructed_change(AnimationProgress progress, void* data) {
  GRect window_bounds = layer_get_bounds(window_get_root_layer(s_window));
  GRect unobstructed_bounds = layer_get_unobstructed_bounds(window_get_root_layer(s_window));
  // move everything up by half the obstruction
  int offset_from_bottom = (window_bounds.size.h - unobstructed_bounds.size.h) / 2;
  window_bounds.size.h-=offset_from_bottom;
  // update layer positions
  #if defined(PBL_PLATFORM_DIORITE)
  layer_set_frame(text_layer_get_layer(s_hrm_layer),GRect(0, window_bounds.size.h/2+12, window_bounds.size.w, 38));
  #endif
#ifdef PBL_PLATFORM_EMERY
  layer_set_frame(text_layer_get_layer(s_hrm_layer),GRect(0, window_bounds.size.h/2+20, window_bounds.size.w, 38));
  layer_set_frame(text_layer_get_layer(s_time_layer),GRect(0, window_bounds.size.h/2-24, window_bounds.size.w, 42));
#else
  layer_set_frame(text_layer_get_layer(s_time_layer),GRect(0, window_bounds.size.h/2-19, window_bounds.size.w, 38));
#endif
  layer_set_frame(text_layer_get_layer(s_step_layer),GRect(0, window_bounds.size.h/2-43, window_bounds.size.w, 38));
}

// Is step data available?
bool step_data_is_available() {
  return HealthServiceAccessibilityMaskAvailable &
    health_service_metric_accessible(HealthMetricStepCount,
      time_start_of_today(), time(NULL));
}

// Daily step goal
static void get_step_goal() {
  const time_t start = time_start_of_today();
  const time_t end = start + SECONDS_PER_DAY;
  s_step_goal = (int)health_service_sum_averaged(HealthMetricStepCount, start, end, HealthServiceTimeScopeDaily);
  //APP_LOG(APP_LOG_LEVEL_DEBUG,"Step goal: %d",s_step_goal);
}

// Todays current step count
static void get_step_count() {
  s_step_count = (int)health_service_sum_today(HealthMetricStepCount);
}

// Current heart rate
static void get_hr() {
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_DIORITE)
  s_hr = (int)health_service_peek_current_value(HealthMetricHeartRateBPM);
#endif
}

// Average daily step count for this time of day
static void get_step_average() {
  const time_t start = time_start_of_today();
  const time_t end = time(NULL);
  s_step_average = (int)health_service_sum_averaged(HealthMetricStepCount, start, end, HealthServiceTimeScopeDaily);
  if(s_step_average>s_step_goal)
    s_step_average=s_step_goal;
  //APP_LOG(APP_LOG_LEVEL_DEBUG,"Step average: %d",s_step_average);
}

static void display_step_count() {
  int thousands = s_step_count / 1000;
  int hundreds = s_step_count % 1000;
  static char s_emoji[5];

  if(s_step_count >= s_step_average) {
    text_layer_set_text_color(s_step_layer, color_winner);
    snprintf(s_emoji, sizeof(s_emoji), "\U0001F60C");
  } else {
    text_layer_set_text_color(s_step_layer, color_loser);
    snprintf(s_emoji, sizeof(s_emoji), "\U0001F4A9");
  }

  if(thousands > 0) {
    snprintf(s_current_steps_buffer, sizeof(s_current_steps_buffer),
      "%d,%03d %s", thousands, hundreds, s_emoji);
  } else {
    snprintf(s_current_steps_buffer, sizeof(s_current_steps_buffer),
      "%d %s", hundreds, s_emoji);
  }

  text_layer_set_text(s_step_layer, s_current_steps_buffer);
}

static void display_heart_rate() {
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_DIORITE)
  if(s_hr>0) {
    snprintf(s_current_hr_buffer,sizeof(s_current_hr_buffer),"%d \U00002764",s_hr);
    text_layer_set_text(s_hrm_layer,s_current_hr_buffer);
    layer_set_hidden(text_layer_get_layer(s_hrm_layer),false);
  } else {
    layer_set_hidden(text_layer_get_layer(s_hrm_layer),true);
  }
#endif
}

static void health_handler(HealthEventType event, void *context) {
  if(event == HealthEventSignificantUpdate) {
    get_step_goal();
  }

  if(event != HealthEventSleepUpdate) {
    get_step_count();
    get_step_average();
    display_step_count();
    get_hr();
    display_heart_rate();
    layer_mark_dirty(s_progress_layer);
    layer_mark_dirty(s_average_layer);
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  strftime(s_current_time_buffer, sizeof(s_current_time_buffer),
           clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);

  text_layer_set_text(s_time_layer, s_current_time_buffer);
}

static void dots_layer_update_proc(Layer *layer, GContext *ctx) {
  const GRect inset = grect_inset(layer_get_bounds(layer), GEdgeInsets(6));

  const int num_dots = 12;
  for(int i = 0; i < num_dots; i++) {
    GPoint pos = gpoint_from_polar(inset, GOvalScaleModeFitCircle,
      DEG_TO_TRIGANGLE(i * 360 / num_dots));
    graphics_context_set_fill_color(ctx, GColorDarkGray);
    graphics_fill_circle(ctx, pos, 2);
  }
}

static void progress_layer_update_proc(Layer *layer, GContext *ctx) {
  const GRect inset = grect_inset(layer_get_bounds(layer), GEdgeInsets(2));
#ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx,
    s_step_count >= s_step_average ? color_winner : color_loser);
#else
graphics_context_set_fill_color(ctx, GColorLightGray);
#endif
  graphics_fill_radial(ctx, inset, GOvalScaleModeFitCircle, 8,
    DEG_TO_TRIGANGLE(0),
    DEG_TO_TRIGANGLE((360 * s_step_count) / s_step_goal));
}

static void average_layer_update_proc(Layer *layer, GContext *ctx) {
  if(s_step_average < 1) {
    return;
  }

  const GRect inset = grect_inset(layer_get_bounds(layer), GEdgeInsets(2));
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow,GColorWhite));

  int trigangle = DEG_TO_TRIGANGLE(360 * s_step_average / s_step_goal);
  int line_width_trigangle = 1000;
  // draw a very narrow radial (it's just a line)
  graphics_fill_radial(ctx, inset, GOvalScaleModeFitCircle, 12,
    trigangle - line_width_trigangle, trigangle);
}

static void window_load(Window *window) {
  GRect window_bounds = layer_get_bounds(window_get_root_layer(window));
  GRect unobstructed_bounds = layer_get_unobstructed_bounds(window_get_root_layer(window));

  // Dots for the progress indicator
  s_dots_layer = layer_create(window_bounds);
  layer_set_update_proc(s_dots_layer, dots_layer_update_proc);
  layer_add_child(s_window_layer, s_dots_layer);

  // Progress indicator
  s_progress_layer = layer_create(window_bounds);
  layer_set_update_proc(s_progress_layer, progress_layer_update_proc);
  layer_add_child(s_window_layer, s_progress_layer);

  // Average indicator
  s_average_layer = layer_create(window_bounds);
  layer_set_update_proc(s_average_layer, average_layer_update_proc);
  layer_add_child(s_window_layer, s_average_layer);

  // move everything up by half the obstruction
  int offset_from_bottom = (window_bounds.size.h - unobstructed_bounds.size.h) / 2;
  window_bounds.size.h-=offset_from_bottom;

  // Create a layer to hold the current time
#ifdef PBL_PLATFORM_EMERY
  s_time_layer = text_layer_create(
    GRect(0, window_bounds.size.h/2-24, window_bounds.size.w, 42));
#else
  s_time_layer = text_layer_create(
      GRect(0, window_bounds.size.h/2-19, window_bounds.size.w, 38));
#endif
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_background_color(s_time_layer, GColorClear);
#ifdef PBL_PLATFORM_EMERY
  text_layer_set_font(s_time_layer,
                    fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
#else
  text_layer_set_font(s_time_layer,
                      fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
#endif
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(s_window_layer, text_layer_get_layer(s_time_layer));

  // Create a layer to hold the current step count
  s_step_layer = text_layer_create(
      GRect(0, window_bounds.size.h/2-43, window_bounds.size.w, 38));
  text_layer_set_background_color(s_step_layer, GColorClear);
  text_layer_set_font(s_step_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_step_layer, GTextAlignmentCenter);
  layer_add_child(s_window_layer, text_layer_get_layer(s_step_layer));

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_DIORITE)
  // Create a layer to hold the current heart rate
#ifdef PBL_PLATFORM_EMERY
  s_hrm_layer = text_layer_create(
      GRect(0, window_bounds.size.h/2+20, window_bounds.size.w, 38));
#else
s_hrm_layer = text_layer_create(
    GRect(0, window_bounds.size.h/2+12, window_bounds.size.w, 38));
#endif
  text_layer_set_background_color(s_hrm_layer, GColorClear);
  text_layer_set_text_color(s_hrm_layer, GColorWhite);
  text_layer_set_font(s_hrm_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_hrm_layer, GTextAlignmentCenter);
  layer_add_child(s_window_layer, text_layer_get_layer(s_hrm_layer));
#endif
  // Subscribe to health events if we can
  if(step_data_is_available()) {
    health_service_events_subscribe(health_handler, NULL);
  }
}

static void window_unload(Window *window) {
  layer_destroy(text_layer_get_layer(s_time_layer));
  layer_destroy(text_layer_get_layer(s_step_layer));
  layer_destroy(s_dots_layer);
  layer_destroy(s_progress_layer);
  layer_destroy(s_average_layer);
}

void init() {
  color_loser = GColorPictonBlue;
  color_winner = GColorJaegerGreen;

  s_window = window_create();
  s_window_layer = window_get_root_layer(s_window);
  window_set_background_color(s_window, GColorBlack);

  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload
  });

  UnobstructedAreaHandlers handlers = {
    // .will_change = unobstructed_will_change,
    .change = unobstructed_change
    // .did_change = unobstructed_did_change
  };
  unobstructed_area_service_subscribe(handlers, NULL);

  window_stack_push(s_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

void deinit() {}

int main() {
  init();
  app_event_loop();
  deinit();
}
