#include <pebble.h>
#include "modules/weather_code_to_resource_id.h"

#define TEMPERATURE_FONT FONT_KEY_GOTHIC_14_BOLD

static Window *s_main_window;
static TextLayer *s_time_layer;
static TextLayer *s_timestamp_layer;
static TextLayer *s_date_layer;

static TextLayer *s_temperature_layer;
static BitmapLayer *s_icon_layer;
static GBitmap *s_icon_bitmap = NULL;

static AppSync s_sync;
static uint8_t s_sync_buffer[64];

enum WeatherKey {
	WEATHER_ICON_KEY = 0x0,         // TUPLE_INT
	WEATHER_TEMPERATURE_KEY = 0x1,  // TUPLE_CSTRING
};

static void sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Sync Error: %d", app_message_error);
}


static void sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "sync_tuple_changed_callback called!");
	switch (key) {
		case WEATHER_ICON_KEY:
			if (s_icon_bitmap) {
				gbitmap_destroy(s_icon_bitmap);
			}

			int32_t weather_id = weather_code_to_resource_id(new_tuple->value->int32);
			if (weather_id < 0) {
				return;
			}
			s_icon_bitmap = gbitmap_create_with_resource(weather_id == 0 ? RESOURCE_ID_WEATHER_UNKNOWN : weather_id);
			bitmap_layer_set_compositing_mode(s_icon_layer, GCompOpSet);
			bitmap_layer_set_bitmap(s_icon_layer, s_icon_bitmap);
			break;

		case WEATHER_TEMPERATURE_KEY:
			// App Sync keeps new_tuple in s_sync_buffer, so we may use it directly
			text_layer_set_text(s_temperature_layer, new_tuple->value->cstring);
			break;
	}
}

static void request_weather(void) {
	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);

	if (!iter) {
		// Error creating outbound message
		APP_LOG(APP_LOG_LEVEL_ERROR, "error creating outbound message!");
		return;
	}

	int value = 1;
	dict_write_int(iter, 1, &value, sizeof(int), true);
	dict_write_end(iter);

	app_message_outbox_send();
}


static void handle_connected(bool connected) {
	if (connected) {
		request_weather();
	}
}


static void handle_minute_tick(struct tm* tick_time, TimeUnits units_changed)
{
	// TODO: Y2038 problem. :-)
	static char s_timestamp_text[] = "2147483648";
	static char s_time_text[] = "00:00";
	static char s_date_text[] = "9999-99-99";
	time_t unix_time = time(NULL);
	long long timestamp = (long long)unix_time;
	struct tm *local_time = localtime(&unix_time);
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

	if (local_time->tm_min % 30 == 0) {
		request_weather();
	}
}

static void main_window_load(Window *window)
{
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_frame(window_layer);
	
	s_time_layer = text_layer_create(GRect(0, 20, bounds.size.w, 28));
	text_layer_set_text_color(s_time_layer, GColorWhite);
	text_layer_set_background_color(s_time_layer, GColorClear);
	text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_LECO_28_LIGHT_NUMBERS ));
	text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

	s_timestamp_layer = text_layer_create(GRect(0, 52, bounds.size.w, 20));
	text_layer_set_text_color(s_timestamp_layer, GColorWhite);
	text_layer_set_background_color(s_timestamp_layer, GColorClear);
	text_layer_set_font(s_timestamp_layer, fonts_get_system_font(FONT_KEY_LECO_20_BOLD_NUMBERS));
	text_layer_set_text_alignment(s_timestamp_layer, GTextAlignmentCenter);
	
	s_date_layer = text_layer_create(GRect(0, 88, bounds.size.w, 14));
	text_layer_set_text_color(s_date_layer, GColorWhite);
	text_layer_set_background_color(s_date_layer, GColorClear);
	text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);

	GSize temp_size = graphics_text_layout_get_content_size(
		"100\u00B0F",
		fonts_get_system_font(TEMPERATURE_FONT),
		GRect(0, 0, bounds.size.w, 20),
		GTextOverflowModeFill,
		GTextAlignmentRight
	);

	int32_t left_offset = (bounds.size.w - (temp_size.w + 32)) >> 1;
	int32_t bottom_offset = bounds.size.h - 52;

	s_icon_layer = bitmap_layer_create(GRect(left_offset, bottom_offset, 32, 32));

	s_temperature_layer = text_layer_create(GRect(left_offset + 32, bottom_offset + ((32 - temp_size.h) >> 1), temp_size.w, 20));
	text_layer_set_text_color(s_temperature_layer, GColorWhite);
	text_layer_set_background_color(s_temperature_layer, GColorClear);
	text_layer_set_font(s_temperature_layer, fonts_get_system_font(TEMPERATURE_FONT));
	text_layer_set_text_alignment(s_temperature_layer, GTextAlignmentRight);

	time_t now = time(NULL);
	struct tm *current_time = localtime(&now);
	handle_minute_tick(current_time, SECOND_UNIT);

	tick_timer_service_subscribe(
		MINUTE_UNIT | HOUR_UNIT | DAY_UNIT | MONTH_UNIT | YEAR_UNIT,
		handle_minute_tick
	);

	Tuplet initial_values[] = {
		TupletInteger(WEATHER_ICON_KEY, (int32_t) 1),
		TupletCString(WEATHER_TEMPERATURE_KEY, "   ?\u00B0F"),
	};

	app_sync_init(&s_sync, s_sync_buffer, sizeof(s_sync_buffer),
			initial_values, ARRAY_LENGTH(initial_values),
			sync_tuple_changed_callback, sync_error_callback, NULL);

	request_weather();

	connection_service_subscribe((ConnectionHandlers) {
		.pebble_app_connection_handler=handle_connected,
		.pebblekit_connection_handler=NULL
	});
	
	layer_add_child(window_layer, text_layer_get_layer(s_timestamp_layer));
	layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
	layer_add_child(window_layer, text_layer_get_layer(s_date_layer));
	layer_add_child(window_layer, bitmap_layer_get_layer(s_icon_layer));
	layer_add_child(window_layer, text_layer_get_layer(s_temperature_layer));
}

static void main_window_unload(Window *window)
{
	connection_service_unsubscribe();
	tick_timer_service_unsubscribe();
	text_layer_destroy(s_timestamp_layer);
	text_layer_destroy(s_time_layer);
	text_layer_destroy(s_date_layer);
	text_layer_destroy(s_temperature_layer);
	bitmap_layer_destroy(s_icon_layer);
	if (s_icon_bitmap) {
		gbitmap_destroy(s_icon_bitmap);
	}
	app_sync_deinit(&s_sync);
}

void handle_init(void)
{
	s_main_window = window_create();
  
	window_set_background_color(s_main_window, COLOR_FALLBACK(GColorOxfordBlue, GColorBlack));

	app_message_open(64, 64);

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

/* vim: set ts=4 sw=4 sts=0 noexpandtab: */
