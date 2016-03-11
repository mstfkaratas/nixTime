#include <pebble.h>

static Window *s_main_window;
static TextLayer *s_time_layer;
static TextLayer *s_timestamp_layer;
static TextLayer *s_date_layer;

static void handle_second_tick(struct tm* tick_time, TimeUnits units_changed)
{
	static char s_timestamp_text[] = "2147483648";
	static char s_time_text[] = "00:00";
	static char s_date_text[] = "9999-99-99";
	time_t unix_time = mktime(tick_time);
	long long timestamp = (long long)unix_time;
	static struct tm *local_time = NULL;
	local_time = localtime(&unix_time);
	if (clock_is_24h_style()) {
		strftime(s_time_text, 8, "%H:%M", local_time);
	} else {
		strftime(s_time_text, 8, "%I:%M", local_time);
	}
	strftime(s_date_text, 12, "%Y-%m-%d", local_time);
	snprintf(s_timestamp_text, 10, "%lld", timestamp);
	text_layer_set_text(s_time_layer, s_time_text);
	text_layer_set_text(s_timestamp_layer, s_timestamp_text);
	text_layer_set_text(s_date_layer, s_date_text);
}

static void main_window_load(Window *window)
{
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_frame(window_layer);
	s_timestamp_layer = text_layer_create(GRect(0, 72, bounds.size.w, 34));
	text_layer_set_text_color(s_timestamp_layer, GColorWhite);
	text_layer_set_background_color(s_timestamp_layer, GColorClear);
	text_layer_set_font(s_timestamp_layer, fonts_get_system_font(FONT_KEY_LECO_20_BOLD_NUMBERS));
	text_layer_set_text_alignment(s_timestamp_layer, GTextAlignmentCenter);
	
	s_time_layer = text_layer_create(GRect(0, 40, bounds.size.w, 34));
	text_layer_set_text_color(s_time_layer, GColorWhite);
	text_layer_set_background_color(s_time_layer, GColorClear);
	text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_LECO_28_LIGHT_NUMBERS ));
	text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
	
	s_date_layer = text_layer_create(GRect(0, 108, bounds.size.w, 34));
	text_layer_set_text_color(s_date_layer, GColorWhite);
	text_layer_set_background_color(s_date_layer, GColorClear);
	text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
	
	time_t now = time(NULL);
	struct tm *current_time = localtime(&now);
	handle_second_tick(current_time, SECOND_UNIT);

	tick_timer_service_subscribe(MINUTE_UNIT, handle_second_tick);
	
	layer_add_child(window_layer, text_layer_get_layer(s_timestamp_layer));
	layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
	layer_add_child(window_layer, text_layer_get_layer(s_date_layer));
}

static void main_window_unload(Window *window)
{
	tick_timer_service_unsubscribe();
	text_layer_destroy(s_timestamp_layer);
	text_layer_destroy(s_time_layer);
}

void handle_init(void)
{
	s_main_window = window_create();
  
	window_set_background_color(s_main_window, COLOR_FALLBACK(GColorOxfordBlue, GColorBlack));

	window_set_window_handlers(s_main_window, (WindowHandlers) {
		.load = main_window_load,
		.unload = main_window_unload,
	});
	window_stack_push(s_main_window, true);
}

void handle_deinit(void)
{
	window_destroy(s_main_window);
}

int main(void)
{
	handle_init();
	app_event_loop();
	handle_deinit();
}
