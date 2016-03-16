#include <pebble.h>
#include "modules/weather_code_to_resource_id.h"
#include "modules/persistence.h"

#define WEATHER_INTERVAL_IN_MINUTES 30
#define MIN_WEATHER_UPDATE_INTERVAL 1700 /* s */

static Window *s_main_window;
static Layer *s_border_layer;
static TextLayer *s_time_layer;
static TextLayer *s_timestamp_layer;
static TextLayer *s_date_layer;
static TextLayer *s_latlng_layer;

static TextLayer *s_temperature_layer;
static BitmapLayer *s_icon_layer;
static GBitmap *s_icon_bitmap = NULL;

static GFont s_time_font;
static GFont s_timestamp_font;
static GFont s_date_font;
static GFont s_temperature_font;
static GFont s_latlng_font;

static AppSync s_sync;
static uint8_t s_sync_buffer[128];

static int s_color_theme;
static GColor s_background_color;
static GColor s_foreground_color;

static int s_temp_unit;
static int s_update_interval = 0;

static int s_start_minute_mod_interval;

enum SyncKey {
	WEATHER_ICON_KEY = 0x0,
	WEATHER_TEMPERATURE_KEY = 0x1,
	WEATHER_LATLNG_KEY = 0x02,
	CONFIG_COLOR_KEY = 0x03,
	CONFIG_TEMP_UNIT_KEY = 0x04,
	CONFIG_UPDATE_INTERVAL_KEY = 0x05,
};

static time_t s_weather_last_updated_time = 0;

/* predeclarations */
int abs(int);
static void request_weather(void);
static void handle_minute_tick(struct tm*, TimeUnits);

/* actual code */

static void sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Sync Error: %d", app_message_error);
}

int abs(int x)
{
	if (x < 0) {
		return -1 * x;
	} else {
		return x;
	}
}

void load_colors() {
	s_color_theme = persist_read_int(PKEY_COLOR);

	s_background_color = color_theme_to_background_color(s_color_theme);
	s_foreground_color = color_theme_to_foreground_color(s_color_theme);
}

void update_colors() {
	window_set_background_color(s_main_window, COLOR_FALLBACK(s_background_color, GColorBlack));
	text_layer_set_text_color(s_latlng_layer, COLOR_FALLBACK(s_foreground_color, GColorWhite));
	layer_mark_dirty(s_border_layer);
}


static void sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "sync_tuple_changed_callback called with key %u", (unsigned int)key);
	s_weather_last_updated_time = time(NULL);
	static char s_latlng_text[] = "+000.0,+000.0";
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

		case WEATHER_LATLNG_KEY:
			{
				uint32_t raw = new_tuple->value->int32;
				if (raw == 0) {
					return;
				}
				int lat = raw >> 16;
				if (lat & 0x8000) {
					lat &= 0x7fff;
					lat *= -1;
				}
				int lng = raw & 0xffff;
				if (lng & 0x8000) {
					lng &= 0x7fff;
					lng *= -1;
				}
				int lat_base = lat / 10;
				int lat_frac = abs(lat % 10);
				int lng_base = lng / 10;
				int lng_frac = abs(lng % 10);
				snprintf(s_latlng_text, 13, "%+3d.%1d,%+3d.%1d", lat_base, lat_frac, lng_base, lng_frac);
				text_layer_set_text(s_latlng_layer, s_latlng_text);
			}
			break;

		case CONFIG_COLOR_KEY:
			persist_write_int(PKEY_COLOR, new_tuple->value->int32);
			load_colors();
			update_colors();
			break;

		case CONFIG_TEMP_UNIT_KEY:
			if (new_tuple->value->int32 == -1) {
				APP_LOG(APP_LOG_LEVEL_DEBUG, "error fetching");
				text_layer_set_text(s_temperature_layer, "...");
			} else if (new_tuple->value->int32 != s_temp_unit) {
				APP_LOG(APP_LOG_LEVEL_DEBUG, "temp_unit changed from %d to %d", (int)s_temp_unit, (int)new_tuple->value->int32);
				s_temp_unit = new_tuple->value->int32;
				persist_write_int(PKEY_TEMP_UNIT, s_temp_unit);
				text_layer_set_text(s_temperature_layer, "...");
				s_weather_last_updated_time = 0;
				request_weather();
			}
			break;

		case CONFIG_UPDATE_INTERVAL_KEY:
			s_update_interval = new_tuple->value->int32;
			persist_write_int(PKEY_UPDATE_INTERVAL, s_update_interval);
			tick_timer_service_subscribe(update_interval_mask(s_update_interval), handle_minute_tick);
			break;
	}
}


static void request_weather(void) {
	DictionaryIterator *iter;

	if ((time(NULL) - s_weather_last_updated_time) < MIN_WEATHER_UPDATE_INTERVAL) {
		return;
	}
	s_weather_last_updated_time = time(NULL);
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
	static char s_timestamp_text[] = "0000000000";
	static char s_time_text[] = "00:00:00";
	static char s_date_text[] = "9999-99-99";
	time_t unix_time = time(NULL);
	struct tm *local_time = localtime(&unix_time);
	if (clock_is_24h_style()) {
		strftime(s_time_text, 8, "%H:%M", local_time);
	} else {
		strftime(s_time_text, 8, "%I:%M", local_time);
	}
	strftime(s_date_text, 12, "%Y-%m-%d", local_time);
	snprintf(s_timestamp_text, 11, "%d", (int)unix_time);
	text_layer_set_text(s_time_layer, s_time_text);
	text_layer_set_text(s_timestamp_layer, s_timestamp_text);
	text_layer_set_text(s_date_layer, s_date_text);

	if (local_time->tm_min % WEATHER_INTERVAL_IN_MINUTES == s_start_minute_mod_interval) {
		request_weather();
	}
}

static void draw_border_layer(Layer* l, GContext* c)
{
	GRect bounds = layer_get_frame(window_get_root_layer(s_main_window));
	graphics_context_set_stroke_color(c, COLOR_FALLBACK(s_foreground_color, GColorWhite));
	graphics_context_set_stroke_width(c, 1);
	graphics_draw_line(c, GPoint(0, 0), GPoint(bounds.size.w * 8, 0));
}

static void main_window_load(Window *window)
{
	/* load initial persisted values */
	if (!persist_exists(PKEY_COLOR)) {
		persist_write_int(PKEY_COLOR, PVALUE_COLOR_THEME_BLUE);
	}
	load_colors();
	if (!persist_exists(PKEY_TEMP_UNIT)) {
		persist_write_int(PKEY_TEMP_UNIT, 'F');
	}
	s_temp_unit = persist_read_int(PKEY_TEMP_UNIT);
	if (!persist_exists(PKEY_UPDATE_INTERVAL)) {
		persist_write_int(PKEY_UPDATE_INTERVAL, PVALUE_UPDATE_INTERVAL_MINUTE);
	}
	s_update_interval = persist_read_int(PKEY_UPDATE_INTERVAL);

	/* initialize UI */
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_frame(window_layer);

	s_time_font = fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS);
	GSize time_size = graphics_text_layout_get_content_size(
		"00:00",
		s_time_font,
		GRect(0, 0, bounds.size.w, bounds.size.h),
		GTextOverflowModeFill,
		GTextAlignmentCenter
	);
	int y_pos = 14;
	s_time_layer = text_layer_create(GRect(0, y_pos, bounds.size.w, time_size.h));
	text_layer_set_text_color(s_time_layer, GColorWhite);
	text_layer_set_background_color(s_time_layer, GColorClear);
	text_layer_set_font(s_time_layer, s_time_font);
	text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
	y_pos += time_size.h;

	s_timestamp_font = fonts_load_custom_font(
		resource_get_handle(RESOURCE_ID_PROGGY_32)
	);
	GSize timestamp_size = graphics_text_layout_get_content_size(
		"2147483648",
		s_timestamp_font,
		GRect(0, 0, bounds.size.w, bounds.size.h),
		GTextOverflowModeFill,
		GTextAlignmentCenter
	);
	s_timestamp_layer = text_layer_create(GRect(0, y_pos, bounds.size.w, timestamp_size.h));
	text_layer_set_text_color(s_timestamp_layer, GColorWhite);
	text_layer_set_background_color(s_timestamp_layer, GColorClear);
	text_layer_set_font(s_timestamp_layer, s_timestamp_font);
	text_layer_set_text_alignment(s_timestamp_layer, GTextAlignmentCenter);
	y_pos += timestamp_size.h;
	y_pos += 4;

	s_date_font = fonts_load_custom_font(
		resource_get_handle(RESOURCE_ID_PROGGY_16)
	);
	GSize date_size = graphics_text_layout_get_content_size(
		"0000-00-00",
		s_date_font,
		GRect(0, 0, bounds.size.w, bounds.size.h),
		GTextOverflowModeFill,
		GTextAlignmentCenter
	);
	s_date_layer = text_layer_create(GRect(0, y_pos, bounds.size.w, 24));
	text_layer_set_text_color(s_date_layer, GColorWhite);
	text_layer_set_background_color(s_date_layer, GColorClear);
	text_layer_set_font(s_date_layer, s_date_font);
	text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
	y_pos += date_size.h;

	int32_t bottom_offset = bounds.size.h - 52;

	s_border_layer = layer_create(GRect(10, bottom_offset, bounds.size.w - 20, 4));
	layer_set_update_proc(s_border_layer, draw_border_layer);
	layer_mark_dirty(s_border_layer);

	s_temperature_font = fonts_load_custom_font(
		resource_get_handle(RESOURCE_ID_LECO_20)
	);
	GSize temp_size = graphics_text_layout_get_content_size(
		"100\u00B0F",
		s_temperature_font,
		GRect(0, 0, bounds.size.w, bounds.size.h),
		GTextOverflowModeFill,
		GTextAlignmentRight
	);

	int32_t left_offset = (bounds.size.w - (temp_size.w + 32)) >> 1;

	s_icon_layer = bitmap_layer_create(GRect(left_offset, bottom_offset, 32, 32));

	s_temperature_layer = text_layer_create(GRect(left_offset + 32, bottom_offset + ((28 - temp_size.h) >> 1), temp_size.w, temp_size.h));
	text_layer_set_text_color(s_temperature_layer, GColorWhite);
	text_layer_set_background_color(s_temperature_layer, GColorClear);
	text_layer_set_font(s_temperature_layer, s_temperature_font);
	text_layer_set_text_alignment(s_temperature_layer, GTextAlignmentRight);

	y_pos = bottom_offset + 26;
	/*s_latlng_font = fonts_load_custom_font(
		resource_get_handle(RESOURCE_ID_PROGGY_12)
	);*/
	s_latlng_font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
	GSize latlng_size = graphics_text_layout_get_content_size(
		"+000.0,+000.0",
		s_latlng_font,
		GRect(0, 0, bounds.size.w, bounds.size.h),
		GTextOverflowModeFill,
		GTextAlignmentCenter
	);
	s_latlng_layer = text_layer_create(GRect(0, y_pos, bounds.size.w, latlng_size.h));
	text_layer_set_background_color(s_latlng_layer, GColorClear);
	text_layer_set_font(s_latlng_layer, s_latlng_font);
	text_layer_set_text_alignment(s_latlng_layer, GTextAlignmentCenter);
	y_pos += latlng_size.h;

	/* set all of our colors based on whatever is in pstorage */
	update_colors();

	/* set up callback stuff below here. DO NOT CREATE NEW UI ELEMENTS */

	Tuplet initial_values[] = {
		TupletInteger(WEATHER_ICON_KEY, (int32_t) 1),
		TupletCString(WEATHER_TEMPERATURE_KEY, "   ?\u00B0F"),
		TupletInteger(WEATHER_LATLNG_KEY, (int32_t) 0),
		TupletInteger(CONFIG_COLOR_KEY, s_color_theme),
		TupletInteger(CONFIG_TEMP_UNIT_KEY, s_temp_unit),
		TupletInteger(CONFIG_UPDATE_INTERVAL_KEY, s_update_interval),
	};

	app_sync_init(&s_sync, s_sync_buffer, sizeof(s_sync_buffer),
			initial_values, ARRAY_LENGTH(initial_values),
			sync_tuple_changed_callback, sync_error_callback, NULL);

	time_t now = time(NULL);
	struct tm *current_time = localtime(&now);

	s_start_minute_mod_interval = current_time->tm_min % WEATHER_INTERVAL_IN_MINUTES;

	handle_minute_tick(current_time, SECOND_UNIT);

	tick_timer_service_subscribe(update_interval_mask(s_update_interval), handle_minute_tick);

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
	layer_add_child(window_layer, text_layer_get_layer(s_latlng_layer));
	layer_add_child(window_layer, s_border_layer);
}

static void main_window_unload(Window *window)
{
	tick_timer_service_unsubscribe();
	connection_service_unsubscribe();
	tick_timer_service_unsubscribe();
	text_layer_destroy(s_timestamp_layer);
	text_layer_destroy(s_time_layer);
	text_layer_destroy(s_date_layer);
	text_layer_destroy(s_temperature_layer);
	text_layer_destroy(s_latlng_layer);
	layer_destroy(s_border_layer);
	bitmap_layer_destroy(s_icon_layer);
	if (s_icon_bitmap) {
		gbitmap_destroy(s_icon_bitmap);
	}
	app_sync_deinit(&s_sync);
}

void handle_init(void)
{
	s_main_window = window_create();

	if (app_message_open(128, 64) != 0) {
		APP_LOG(APP_LOG_LEVEL_ERROR, "could not init app_message_buffer");
	}

	window_set_window_handlers(s_main_window, (WindowHandlers) {
		.load = main_window_load,
		.unload = main_window_unload,
	});
	window_stack_push(s_main_window, true);
}

void handle_deinit(void)
{
	app_message_deregister_callbacks();
	window_destroy(s_main_window);
}

int main(void)
{
	handle_init();
	app_event_loop();
	handle_deinit();
}

/* vim: set ts=4 sw=4 sts=0 noexpandtab: */
