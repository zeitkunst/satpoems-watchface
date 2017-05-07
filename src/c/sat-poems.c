// To convert from OTF to TTF:
//  fontforge -script scripts/otf2ttf.sh FONTNAME.otf 
//
// Working with more than 3-byte unicode glyphs:
// https://forums.pebble.com/t/how-can-i-filter-3-byte-unicode-characters-glyphs-in-ttf/26024
//
// TODO
// * Figure out how to properly calculate the size of the scroll window for a given font size, have to take into account descenders, need to ensure that we scroll by an integral amount of maximum height + descenders
// * I like Fell English at 24, but need to recalculate the sizes accordingly
// * Try different fonts, sans-serif fonts too, also for the time line below 
// * Try text for time too
// * Generative poem, for Lisa only, that takes pyephem data for Sun, Moon, planets, asteroids (!), and creates poem based off of that

/*
 * State machine for these poems:
 * 1. Start screen (blank for a period or so)
 * 2. Title screen (a couple of periods)
 * 3. Blank screen (for a period or two)
 * 4. Poem, scroll from top to bottom (we might want to split out to another state machine here, for the scrolling, and/or for the scrolling of individual screens if we decide to do things that way)
 * 5. Blank screen (a couple of periods)
 * 6. Restart
 */

#include <pebble.h>
#define TIMER_PERIOD 500

// State machine variables for satellite poem
typedef enum
{
    STATE_START = 0,
    STATE_TITLE,
    STATE_BLANK_1,
    STATE_POEM,
    STATE_BLANK_2
} satellite_state_t;




satellite_state_t satellite_state = STATE_START;
static uint8_t current_period = 0;

// Margin for text, in pixels
uint8_t margin = 4;

// Layers and fonts
static Window *s_main_window;
static Layer *window_layer;

static TextLayer *s_time_layer;
static GFont s_time_font;

static TextLayer *s_poem_layer;
static GFont s_poem_font;

static TextLayer *s_title_layer = NULL;
static GFont s_title_font;
static const char *default_title = "A SATELLITE POEM";

static ScrollLayer *s_scroll_layer;

// Screen bounds
static GRect bounds;

// Calculations for scrolling based on font size
// Probably need to change when we change fonts for text on scroll layer
// Size 24: 24 pixels high plus around 8 for descenders, then 24 pixels for each line, and -1 to make pixel "perfect"
static int fontSize = 24; // in pixels (?)
static int descenderSize = 6; // in pixels (?)
static int numLines = 5; // total number of lines we'd like to display on screen at this font size
int scrollSize, pageScroll;

// Keeping track of state time
AppTimer *stateTimer; // Timer for checking our state
int currentStateTime = 0; // Counter for keeping track of time

// Duration of periods
static uint8_t updatePeriod = 4; // Scroll every updatePeriod seconds
//static uint8_t updatePeriod = 1; // Scroll every updatePeriod seconds
static uint8_t poemPeriod  = 10; // Update poem every poemPeriod minutes
//static uint8_t poemPeriod  = 1; // Update poem every poemPeriod minutes

// TODO
// These periods are tied to the updatePeriod variable...I think we need to change it to be based on milliseconds or some other time-based value
uint8_t state_periods[] = {
    1,
    1,
    1,
    0, // we don't move forward in STATE_POEM based on periods
    1
};

// Amount of time to stay in each state, based on the granularity of the timer
int state_times[] = {
    1 * TIMER_PERIOD,
    4 * TIMER_PERIOD,
    1 * TIMER_PERIOD,
    8 * TIMER_PERIOD, 
    1 * TIMER_PERIOD
};



// Buffers for incoming information
static char title_buffer[256] = {0};
static char title_layer_buffer[256] = {0};

static char poem_buffer[2048];
static char poem_layer_buffer[2048];

/*
 * Create title layer with optional default title
 */
static void generate_title_layer(char *title) {
    uint8_t text_height = 20 + 8 + 20 + 20;
    s_title_layer = text_layer_create(
            GRect(margin, PBL_IF_ROUND_ELSE(84 - (text_height/2), 84 - (text_height/2)), bounds.size.w - (2 * margin), text_height));

    //s_title_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ADOBE_JENSON_24));
    s_title_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_CHARIS_SIL_24));

    // Improve the layout to be more like a watchface
    text_layer_set_background_color(s_title_layer, GColorClear);
    text_layer_set_text_color(s_title_layer, GColorWhite);

    if (title_layer_buffer[0] == 0) {
        text_layer_set_text(s_title_layer, title);
    } else {
        text_layer_set_text(s_title_layer, title_layer_buffer);
    }
    text_layer_set_font(s_title_layer, s_title_font);
    text_layer_set_text_alignment(s_title_layer, GTextAlignmentCenter);

    // Add it as a child layer to the Window's root layer
    layer_add_child(window_layer, text_layer_get_layer(s_title_layer));
}

/*
 * Methods for showing and hiding text and scroll layers
 */
static void show_title_layer(void) {
    layer_set_hidden((Layer *)s_title_layer, false);
}


static void hide_scroll_layer(void) {
    layer_set_hidden((Layer *)s_scroll_layer, true);
}

static void show_scroll_layer(void) {
    layer_set_hidden((Layer *)s_scroll_layer, false);
}


static void hide_title_layer(void) {
    layer_set_hidden((Layer *)s_title_layer, true);
}

/*
 * Update time and write to buffer
 */
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


/*
 * Scroll text area forward each updatePeriod
 */
static void scroll_poem(void) {
    int absOffset;

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
        scroll_layer_set_content_offset(s_scroll_layer, GPoint(0, content_offset.y - pageScroll), true);
    } else {
        // We seem to be scrolled somehow
        // Check and see if we should continue scrolling
        // We continue scrolling if our absOffset + our scrolling page 
        // is >= our content size
        if ((absOffset + pageScroll + descenderSize) >= (int)content_size.h) {
            // Return to start
            //APP_LOG(APP_LOG_LEVEL_INFO, "HERE");
            scroll_layer_set_content_offset(s_scroll_layer, GPoint(0, 0), true);
            hide_scroll_layer();
            satellite_state = STATE_BLANK_2;

        } else {
            scroll_layer_set_content_offset(s_scroll_layer, GPoint(0, content_offset.y - pageScroll), true);
        }

    }

}

/**
 * Timer that gets called every TIMER_PERIOD milliseconds to deal with state changes
 */
static void stateTimerCallback(void *data) {

    switch (satellite_state) {
        case STATE_START:
            if (currentStateTime < state_times[STATE_START]) {

                currentStateTime += TIMER_PERIOD;
            } else {
                currentStateTime = 0;
                show_title_layer();
                satellite_state = STATE_TITLE;
            }
            break;
        case STATE_TITLE:
            if (currentStateTime < state_times[STATE_TITLE]) {
                currentStateTime += TIMER_PERIOD;
            } else {
                currentStateTime = 0;
                hide_title_layer();
                satellite_state = STATE_BLANK_1;
                updatePeriod = 4;
            }
            break;
        case STATE_BLANK_1:
            if (currentStateTime < state_times[STATE_BLANK_1]) {
                currentStateTime += TIMER_PERIOD;
            } else {
                currentStateTime = 0;
                show_scroll_layer();
                satellite_state = STATE_POEM;
                updatePeriod = 6;
            }

            break;
        case STATE_POEM:

            // Before scrolling, we could probably change our updatePeriod
            // to something different than the other periods
            if (currentStateTime < state_times[STATE_POEM]) {
                currentStateTime += TIMER_PERIOD;
            } else {
                scroll_poem();
                currentStateTime = 0;
            }


            break;
        case STATE_BLANK_2:
            if (currentStateTime < state_times[STATE_BLANK_2]) {
                currentStateTime += TIMER_PERIOD;
            } else {
                currentStateTime = 0;
                satellite_state = STATE_START;
                updatePeriod = 4;
            }

            break;
        
    }



    // Register timer for next period
    stateTimer = app_timer_register(TIMER_PERIOD, (AppTimerCallback) stateTimerCallback, NULL);
}


/*
 * Main handler for moving through our state machine, updating time, etc.
 */
static void tick_handler_seconds(struct tm *tick_time, TimeUnits units_changed) {

    // Update every updatePeriod seconds
    // We will probably be able to delete the following given our refactoring
    /*
    if (tick_time->tm_sec % updatePeriod == 0) {
        
        APP_LOG(APP_LOG_LEVEL_INFO, "Current state: %d", satellite_state);
        switch (satellite_state) {
            case STATE_START:
                if (current_period < state_periods[STATE_START]) {
                    current_period += 1;
                } else {
                    current_period = 0;
                    show_title_layer();
                    satellite_state = STATE_TITLE;
                    updatePeriod = 6;
                }
                break;
            case STATE_TITLE:
                if (current_period < state_periods[STATE_TITLE]) {
                    current_period += 1;
                } else {
                    current_period = 0;
                    hide_title_layer();
                    satellite_state = STATE_BLANK_1;
                    updatePeriod = 4;
                }
                break;
            case STATE_BLANK_1:
                if (current_period < state_periods[STATE_BLANK_1]) {
                    current_period += 1;
                } else {
                    current_period = 0;
                    show_scroll_layer();
                    satellite_state = STATE_POEM;
                    updatePeriod = 6;
                }

                break;
            case STATE_POEM:

                // Before scrolling, we could probably change our updatePeriod
                // to something different than the other periods
                scroll_poem();

                break;
            case STATE_BLANK_2:
                if (current_period < state_periods[STATE_BLANK_2]) {
                    current_period += 1;
                } else {
                    current_period = 0;
                    satellite_state = STATE_START;
                    updatePeriod = 4;
                }

                break;
            
        }

    } 
    */

    // Update time every minute    
    if (tick_time->tm_sec % 60 == 0) {
        update_time();
        APP_LOG(APP_LOG_LEVEL_INFO, "updating time");
    }

    // TODO
    // Switch from repretoire of poems, maybe a set of three (?) every few minutes
    // Update satellites overhead every 5-15 minutes
    // In poem, talk about the time the satellite was overhead

    // Update poem every poemPeriod minutes
    // Check the minute
    if (tick_time->tm_min % poemPeriod == 0) {
        // and then ensure that we only do this once during the minute
        if (tick_time->tm_sec % 60 == 0) {
            // Begin dictionary
            DictionaryIterator *iter;
            app_message_outbox_begin(&iter);
    
            // Add a key-value pair
            dict_write_uint8(iter, 0, 0);
    
            app_message_outbox_send();

            // Reset scroll layer
            scroll_layer_set_content_offset(s_scroll_layer, GPoint(0, 0), true);
            APP_LOG(APP_LOG_LEVEL_INFO, "Updating poem");
        }
    }

}

/*
 * Set up window, layers, and fonts
 */
static void main_window_load(Window *window) {

    //static int scrollSize = 32 + 24 + 24 + 24 + 24 - 1;
    scrollSize = (fontSize + descenderSize) + (numLines - 1) * fontSize;
    // The amount we scroll is based on the point size, and does not include the descenders
    //static int pageScroll = 5 * 24;
    pageScroll = numLines * fontSize;

   
    // Get information about the window
    window_layer = window_get_root_layer(window);
    bounds = layer_get_frame(window_layer);
    GRect max_text_bounds = GRect(0, 0, bounds.size.w - (margin * 2), 2000);

    // Create time GFont
    s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ANDIKA_20));
    //s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DELICIOUS_20));

    // Create poem font
    //s_poem_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_IMFELL_ENGLISH_28));
    //s_poem_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ADOBE_JENSON_24));
    s_poem_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_CHARIS_SIL_24));
    //s_poem_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_PERFECT_DOS_20));

    // Create scroll layer
    s_scroll_layer = scroll_layer_create(GRect(margin, PBL_IF_ROUND_ELSE(margin + 5, margin), bounds.size.w - (margin * 2), scrollSize));
    //s_scroll_layer = scroll_layer_create(bounds);

    // This binds the scroll layer to the window so that up/down map to scrolling
    scroll_layer_set_click_config_onto_window(s_scroll_layer, window);
    scroll_layer_set_shadow_hidden(s_scroll_layer, true);

    generate_title_layer("SATELLITE POEMS");
    hide_title_layer();

    // Create poem Layer
    // TODO: Work on margins, readable font size
    
    //s_poem_layer = text_layer_create(GRect(margin, PBL_IF_ROUND_ELSE(margin + 5, margin), bounds.size.w - (margin * 2), 140));
    s_poem_layer = text_layer_create(max_text_bounds);

    // Style poem layer text
    text_layer_set_background_color(s_poem_layer, GColorClear);
    text_layer_set_text_color(s_poem_layer, GColorWhite);
#if defined(PBL_ROUND)
    text_layer_set_text_alignment(s_poem_layer, GTextAlignmentCenter);
#endif
    text_layer_set_overflow_mode(s_poem_layer, GTextOverflowModeWordWrap);
    //text_layer_set_text(s_poem_layer, "This is a longish poem just a test nothing more. And here is some more text it's super long isn't it there just isn't much else that we can do here. Oh but we can continue going can't we and make this as long as we'd like oh yes we can oh yes we can we just continue going and going and going like that bunny yes we can oh yes yes yes.");
    //text_layer_set_text(s_poem_layer, "This is a longish poem just a test nothing more. ");
    text_layer_set_text(s_poem_layer, "Waiting to know the objects above...");
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
    hide_scroll_layer();

#if defined(PBL_ROUND)
    text_layer_enable_screen_text_flow_and_paging(s_poem_layer, 4);
#endif

    // Create default title
    snprintf(title_layer_buffer, sizeof(title_layer_buffer), "%s", default_title);

    // Register state timer callback
    stateTimer = app_timer_register(TIMER_PERIOD, (AppTimerCallback) stateTimerCallback, NULL);
}

/*
 * Destory variables so that we don't leak memory
 */
static void main_window_unload(Window *window) {
    // Destory TextLayer
    text_layer_destroy(s_time_layer);

    // Unload GFont
    fonts_unload_custom_font(s_time_font);

    // Destroy poem elements
    text_layer_destroy(s_poem_layer);
    fonts_unload_custom_font(s_poem_font);

    // Destroy scroll layer
    scroll_layer_destroy(s_scroll_layer);

    // Destroy title elements
    text_layer_destroy(s_title_layer);
    fonts_unload_custom_font(s_title_font);

    // Destroy timer
    app_timer_cancel(stateTimer);
}

/*
 * Handle incoming data from phone/javascript
 */
static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
    // Read tuples for data
    Tuple *poem_tuple = dict_find(iterator, MESSAGE_KEY_POEM);
    Tuple *title_tuple = dict_find(iterator, MESSAGE_KEY_TITLE);


    if (poem_tuple && title_tuple) {
        APP_LOG(APP_LOG_LEVEL_INFO, "HAVE POEM TUPLE");
        snprintf(poem_buffer, sizeof(poem_buffer), "%s", poem_tuple->value->cstring);
        snprintf(title_buffer, sizeof(title_buffer), "%s", title_tuple->value->cstring);


        // Assemble string and display
        snprintf(poem_layer_buffer, sizeof(poem_layer_buffer), "%s", poem_buffer);
        text_layer_set_text(s_poem_layer, poem_layer_buffer);

        // Assemble title string
        snprintf(title_layer_buffer, sizeof(title_layer_buffer), "%s", title_buffer);
        text_layer_set_text(s_title_layer, title_layer_buffer);
        APP_LOG(APP_LOG_LEVEL_INFO, "TITLE: %s", title_buffer);

        // TODO
        // Calculate all of this properly based off of the size of the text + descenders and such
        //GSize content_size = text_layer_get_content_size(s_poem_layer);
    GSize content_size = graphics_text_layout_get_content_size(
            text_layer_get_text(s_poem_layer),
            s_poem_font,
            GRect(0, 0, bounds.size.w - (margin * 2), 5000),
            GTextAlignmentLeft,
            GTextOverflowModeWordWrap);

        // TODO: REMEMBER that we have to add the descender height to the total content size height
        content_size.h = content_size.h + 8 + 20;
        GPoint content_offset = scroll_layer_get_content_offset(s_scroll_layer);
        text_layer_set_size(s_poem_layer, content_size);
        scroll_layer_set_content_size(s_scroll_layer, GSize(bounds.size.w, scrollSize * (((int)content_size.h/scrollSize) + 1)));

        APP_LOG(APP_LOG_LEVEL_INFO, "initial y content offset: %d", (int)content_offset.y);
        APP_LOG(APP_LOG_LEVEL_INFO, "initial y content size: %d", (int)content_size.h);
        APP_LOG(APP_LOG_LEVEL_INFO, "total size of content: %d", scrollSize * (((int)content_size.h/scrollSize) + 1));


    }
}

/*
 * Deal with callback and inbox issues
 */
static void inbox_dropped_callback(AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped! Reason: %d", (int)reason);
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

/*
 * Initialize window, callbacks, handlers
 */
static void init() {
    s_main_window = window_create();

    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load = main_window_load,
        .unload = main_window_unload
    });    

    window_set_background_color(s_main_window, GColorBlack);

    // Only one subscription to this service is allowed
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

/*
 * Destroy window
 */
static void deinit() {
    // Destroy window
    window_destroy(s_main_window);
}

/*
 * Main app loop
 */
int main(void) {
    init();
    app_event_loop();
    deinit();
}
