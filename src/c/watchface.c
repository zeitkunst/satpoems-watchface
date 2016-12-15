#include <pebble.h>

static Window *s_main_window;

static TextLayer *s_time_layer;

static TextLayer *s_weather_layer;
static TextLayer *s_poem_layer;

static ScrollLayer *s_scroll_layer;

static GFont s_time_font;
static GFont s_weather_font;
static GFont s_poem_font;

static GRect bounds;

static int pageScroll = 140;

static uint8_t scrollPeriod = 10; // Scroll every scrollPeriod seconds
static uint8_t poemPeriod  = 10; // Update poem every poemPeriod seconds


bool scrollForwards = true;

// Store incoming information
static char temperature_buffer[8];
static char conditions_buffer[32];
static char weather_layer_buffer[32];
static char poem_buffer[2048];
static char poem_layer_buffer[2048];

static void update_time() {
    // Get a tm structure
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);

    // Write the current hours and minutes into a buffer
    static char s_buffer[8];
    strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);

    // Display this time on the TextLayer
    text_layer_set_text(s_time_layer, s_buffer);
}

static void tick_handler_seconds(struct tm *tick_time, TimeUnits units_changed) {
    int absOffset;

    // Scroll every 10 seconds
    if (tick_time->tm_sec % scrollPeriod == 0) {
        GSize content_size = scroll_layer_get_content_size(s_scroll_layer);
        GPoint content_offset = scroll_layer_get_content_offset(s_scroll_layer);

        APP_LOG(APP_LOG_LEVEL_INFO, "y content offset: %d", (int)content_offset.y);
        APP_LOG(APP_LOG_LEVEL_INFO, "y content size: %d", (int)content_size.h);
        APP_LOG(APP_LOG_LEVEL_INFO, "Scrolling by a page");


        absOffset = (int)content_offset.y;
        if (absOffset < 0) {
            absOffset = absOffset * -1;
        }

        // This is probably horribly inefficient, need to see if there is something better
        // If we're at the top, scroll by a page
        if ((int)content_offset.y == 0) {
            scrollForwards = true;
            scroll_layer_set_content_offset(s_scroll_layer, GPoint(0, content_offset.y - pageScroll), true);
        } else {
            // We seem to be scrolled somehow
            // Check and see if we should continue scrolling
            // We continue scrolling if our absOffset + our scrolling page 
            // is >= our content size
            if ((absOffset + pageScroll) >= (int)content_size.h) {
                // Return to start
                APP_LOG(APP_LOG_LEVEL_INFO, "HERE");
                scroll_layer_set_content_offset(s_scroll_layer, GPoint(0, 0), true);

                /*
                // Scroll backwards
                scrollForwards = false;
                scroll_layer_set_content_offset(s_scroll_layer, GPoint(0, content_offset.y + pageScroll), true);
                */
            } else {
                scroll_layer_set_content_offset(s_scroll_layer, GPoint(0, content_offset.y - pageScroll), true);
            }

            /*
             * I think this is cruft...
            if (!scrollForwards) {
                scroll_layer_set_content_offset(s_scroll_layer, GPoint(0, content_offset.y + pageScroll), true);

            } else {
                // Continue scrolling forwards
                scroll_layer_set_content_offset(s_scroll_layer, GPoint(0, content_offset.y - pageScroll), true);

            }
            */

        }

        

        //scroll_layer_set_content_offset(s_scroll_layer, GPoint(0, content_offset.y - 140), true);

    } 

    // Update time every minute    
    if (tick_time->tm_sec % 60 == 0) {
        update_time();
        APP_LOG(APP_LOG_LEVEL_INFO, "updating time");
    }

    // TODO
    // Switch from repretoire of poems, maybe a set of three (?) every few minutes
    // Update satellites overhead every 5-15 minutes
    // In poem, talk about the time the satellite was overhead

    // Update poem every 10 minutes or so
    if (tick_time->tm_min % poemPeriod == 0) {
        // Begin dictionary
        DictionaryIterator *iter;
        app_message_outbox_begin(&iter);

        // Add a key-value pair
        dict_write_uint8(iter, 0, 0);

        app_message_outbox_send();
        APP_LOG(APP_LOG_LEVEL_INFO, "Updating poem");
    }

}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
    update_time();

    /*
    // This is how we scroll automatically without user intervention!
    // Need to refine it...
    GSize content_size = scroll_layer_get_content_size(s_scroll_layer);
    GPoint content_offset = scroll_layer_get_content_offset(s_scroll_layer);

    scroll_layer_set_content_offset(s_scroll_layer, GPoint(0, content_offset.y - 40), true);
    */

}

static void main_window_load(Window *window) {
    uint8_t margin = 4;
   
    // Get information about the window
    Layer *window_layer = window_get_root_layer(window);
    bounds = layer_get_frame(window_layer);
    GRect max_text_bounds = GRect(0, 0, bounds.size.w - (margin * 2), 2000);

    // Create time GFont
    s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_PERFECT_DOS_20));

    // Create weather font
    s_weather_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_PERFECT_DOS_20));

    // Create poem font
    s_poem_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_IMFELL_PICA_28));
    //s_poem_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_PERFECT_DOS_20));

    // Create scroll layer
    s_scroll_layer = scroll_layer_create(GRect(margin, PBL_IF_ROUND_ELSE(margin + 5, margin), bounds.size.w - (margin * 2), pageScroll));
    //s_scroll_layer = scroll_layer_create(bounds);

    // This binds the scroll layer to the window so that up/down map to scrolling
    scroll_layer_set_click_config_onto_window(s_scroll_layer, window);
    scroll_layer_set_shadow_hidden(s_scroll_layer, true);

    /*
    // Create temperature Layer
    s_weather_layer = text_layer_create(GRect(0, PBL_IF_ROUND_ELSE(125, 120), bounds.size.w, 25));
    
    // Style the weather layer text
    text_layer_set_background_color(s_weather_layer, GColorClear);
    text_layer_set_text_color(s_weather_layer, GColorWhite);
    text_layer_set_text_alignment(s_weather_layer, GTextAlignmentCenter);
    text_layer_set_text(s_weather_layer, "Some weather...");
    text_layer_set_font(s_weather_layer, s_weather_font);

    // Add weather layer to window
    //layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_weather_layer));
    */

    // Create poem Layer
    // TODO: Work on margins, readable font size
    
    //s_poem_layer = text_layer_create(GRect(margin, PBL_IF_ROUND_ELSE(margin + 5, margin), bounds.size.w - (margin * 2), 140));
    s_poem_layer = text_layer_create(max_text_bounds);

    // Style poem layer text
    text_layer_set_background_color(s_poem_layer, GColorClear);
    text_layer_set_text_color(s_poem_layer, GColorWhite);
    //text_layer_set_text_alignment(s_poem_layer, GTextAlignmentCenter);
    text_layer_set_overflow_mode(s_poem_layer, GTextOverflowModeWordWrap);
    //text_layer_set_text(s_poem_layer, "This is a longish poem just a test nothing more. And here is some more text it's super long isn't it there just isn't much else that we can do here. Oh but we can continue going can't we and make this as long as we'd like oh yes we can oh yes we can we just continue going and going and going like that bunny yes we can oh yes yes yes.");
    //text_layer_set_text(s_poem_layer, "This is a longish poem just a test nothing more. ");
    text_layer_set_text(s_poem_layer, "");
    text_layer_set_font(s_poem_layer, s_poem_font);




    // Create the text layer with specific bounds
    s_time_layer = text_layer_create(
            GRect(margin, PBL_IF_ROUND_ELSE(144, 144), bounds.size.w - (2 * margin), 20));

    // Improve the layout to be more like a watchface
    text_layer_set_background_color(s_time_layer, GColorClear);
    text_layer_set_text_color(s_time_layer, GColorWhite);
    text_layer_set_text(s_time_layer, "00:00");
    //text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
    text_layer_set_font(s_time_layer, s_time_font);
    text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);


    // Add layers for display
    scroll_layer_add_child(s_scroll_layer, text_layer_get_layer(s_poem_layer));


    // Add poem layer to window
    //layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_poem_layer));
    

    // Get sizes of text layer and update scroll layer content size
    //GSize content_size = text_layer_get_content_size(s_poem_layer);
    //text_layer_set_size(s_poem_layer, content_size);
    //scroll_layer_set_content_size(s_scroll_layer, GSize(bounds.size.w, content_size.h + 4));


    // Add it as a child layer to the Window's root layer
    layer_add_child(window_layer, text_layer_get_layer(s_time_layer));

    // Add scroll layer to window
    layer_add_child(window_layer, scroll_layer_get_layer(s_scroll_layer));

}

static void main_window_unload(Window *window) {
    // Destory TextLayer
    text_layer_destroy(s_time_layer);

    // Unload GFont
    fonts_unload_custom_font(s_time_font);

    // Destroy weather elements
    text_layer_destroy(s_weather_layer);
    fonts_unload_custom_font(s_weather_font);

    // Destroy poem elements
    text_layer_destroy(s_poem_layer);
    fonts_unload_custom_font(s_poem_font);

    // Destroy scroll layer
    scroll_layer_destroy(s_scroll_layer);
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
    // Read tuples for data
    Tuple *temp_tuple = dict_find(iterator, MESSAGE_KEY_TEMPERATURE);
    Tuple *conditions_tuple = dict_find(iterator, MESSAGE_KEY_CONDITIONS);
    Tuple *poem_tuple = dict_find(iterator, MESSAGE_KEY_POEM);

    // If all data available, use it
    if (temp_tuple && conditions_tuple) {
        snprintf(temperature_buffer, sizeof(temperature_buffer), "%dC", (int)temp_tuple->value->int32);
        snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", conditions_tuple->value->cstring);

        // Assemble full string and display
        snprintf(weather_layer_buffer, sizeof(weather_layer_buffer), "%s, %s", temperature_buffer, conditions_buffer);
        text_layer_set_text(s_weather_layer, weather_layer_buffer);
    }

    if (poem_tuple) {
        snprintf(poem_buffer, sizeof(poem_buffer), "%s", poem_tuple->value->cstring);

        // Assemble string and display
        snprintf(poem_layer_buffer, sizeof(poem_layer_buffer), "%s", poem_buffer);
        text_layer_set_text(s_poem_layer, poem_layer_buffer);

        GSize content_size = text_layer_get_content_size(s_poem_layer);
        GPoint content_offset = scroll_layer_get_content_offset(s_scroll_layer);
        text_layer_set_size(s_poem_layer, content_size);
        scroll_layer_set_content_size(s_scroll_layer, GSize(bounds.size.w, content_size.h + 4));
        APP_LOG(APP_LOG_LEVEL_INFO, "initial y content offset: %d", (int)content_offset.y);
        APP_LOG(APP_LOG_LEVEL_INFO, "initial y content size: %d", (int)content_size.h);


    }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped! Reason: %d", (int)reason);

    /*
     * This doesn't seem to work
    if (reason == APP_MSG_BUSY) {
        window_stack_pop_all(false);
    }
    */

}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

static void init() {
    s_main_window = window_create();

    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load = main_window_load,
        .unload = main_window_unload
    });    

    window_set_background_color(s_main_window, GColorBlack);

    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
    tick_timer_service_subscribe(SECOND_UNIT, tick_handler_seconds);

    window_stack_push(s_main_window, true);

    // Register callbacks
    app_message_register_inbox_received(inbox_received_callback);
    app_message_register_inbox_dropped(inbox_dropped_callback);
    app_message_register_outbox_failed(outbox_failed_callback);
    app_message_register_outbox_sent(outbox_sent_callback);

    // Open AppMessage
    const int inbox_size = 4096;
    const int outbox_size = 4096;
    app_message_open(inbox_size, outbox_size);

    // Make sure the time is displayed from the start
    update_time();
}

static void deinit() {
    // Destroy window
    window_destroy(s_main_window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
