/*
 * Morpheuz Sleep Monitor
 *
 * Copyright (c) 2013-2016 James Fowler
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "pebble.h"
#include "morpheuz.h"
#include "language.h"
  
#ifndef PBL_PLATFORM_APLITE

#define NUM_MENU_SECTIONS 1
#define NO_PRESETS 3

// Private
static Window *window;
static MenuLayer *menu_layer;
static char menu_text[TIME_RANGE_LEN];
static int16_t selected_row;
static int16_t centre;
static int16_t width;

// Change the version only if the PresetData structure changes
#define PRESET_VER 42
typedef struct {
  uint8_t preset_ver;
  uint8_t fromhr[NO_PRESETS];
  uint8_t frommin[NO_PRESETS];
  uint8_t tohr[NO_PRESETS];
  uint8_t tomin[NO_PRESETS];
  uint32_t from[NO_PRESETS];
  uint32_t to[NO_PRESETS];
} PresetData;

uint32_t sort_to[NO_PRESETS];
uint8_t sort_no[NO_PRESETS];

static PresetData preset_data;

/*
 * Save the present data structure
 */
static void save_preset_data() {
  LOG_DEBUG("save_preset_data (%d)", sizeof(preset_data));
  int written = persist_write_data(PERSIST_PRESET_KEY, &preset_data, sizeof(preset_data));
  if (written != sizeof(preset_data)) {
    LOG_ERROR("save_preset_data error (%d)", written);
  }
}

/*
 * Clear preset if needed
 */
static void clear_preset_data() {
  memset(&preset_data, 0, sizeof(preset_data));
  preset_data.preset_ver = PRESET_VER;
  for (uint8_t i = 0; i < NO_PRESETS; i++) {
    preset_data.fromhr[i] = FROM_HR_DEF;
    preset_data.frommin[i] = FROM_MIN_DEF;
    preset_data.tohr[i] = TO_HR_DEF;
    preset_data.tomin[i] = TO_MIN_DEF;
    preset_data.from[i] = to_mins(FROM_HR_DEF,FROM_MIN_DEF);
    preset_data.to[i] = to_mins(TO_HR_DEF,TO_MIN_DEF);
  }
}

/*
 * Read the preset data (or create it if missing)
 */
static void read_preset_data() {
  int read = persist_read_data(PERSIST_PRESET_KEY, &preset_data, sizeof(preset_data));
  if (read != sizeof(preset_data) || preset_data.preset_ver != PRESET_VER) {
    clear_preset_data();
  }
}

/*
 * A callback is used to specify the amount of sections of menu items
 * With this, you can dynamically add and remove sections
 */
static uint16_t menu_get_num_sections_callback(MenuLayer *menu_layer, void *data) {
  return NUM_MENU_SECTIONS;
}

/*
 * Each section has a number of items;  we use a callback to specify this
 * You can also dynamically add and remove items using this
 */
static uint16_t menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  return NO_PRESETS;
}

/*
 * A callback is used to specify the height of the section header
 */
static int16_t menu_get_header_height_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}



/*
 * Here we draw what each header is
 */
static void menu_draw_header_callback(GContext* ctx, const Layer *cell_layer, uint16_t section_index, void *data) {
  graphics_context_set_text_color(ctx, MENU_HEAD_COLOR);
  graphics_draw_text(ctx, MENU_PRESET, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GRect(0, -2, width, 32), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

/*
 * This is the menu item draw callback where you specify what each item should look like
 */
static void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {

  int16_t index = cell_index->row;
 
  copy_time_range_into_field(menu_text, sizeof(menu_text), preset_data.fromhr[index], preset_data.frommin[index], preset_data.tohr[index], preset_data.tomin[index]);
  
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  
  graphics_context_set_text_color(ctx, MENU_TEXT_COLOR);
  
  graphics_draw_text(ctx, menu_text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), GRect(0, 4, width, 32), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

}

/*
 * Do menu action after shutting the menu and allowing time for the animations to complete
 */
static void do_menu_action(void *data) {
  get_config_data()->fromhr = preset_data.fromhr[selected_row];
  get_config_data()->frommin = preset_data.frommin[selected_row];
  get_config_data()->tohr = preset_data.tohr[selected_row];
  get_config_data()->tomin = preset_data.tomin[selected_row];
  get_config_data()->from = preset_data.from[selected_row];
  get_config_data()->to = preset_data.to[selected_row];    
  get_config_data()->smart = true;
  resend_all_data(true); // Force resend - we've fiddled with the times
  trigger_config_save();
  set_smart_status();
}

/*
 * Do menu action after shutting the menu and allowing time for the animations to complete
 */
static void do_long_menu_action(void *data) {
  preset_data.fromhr[selected_row] = get_config_data()->fromhr;
  preset_data.frommin[selected_row] = get_config_data()->frommin;
  preset_data.tohr[selected_row] = get_config_data()->tohr;
  preset_data.tomin[selected_row] = get_config_data()->tomin;
  preset_data.from[selected_row] = get_config_data()->from;
  preset_data.to[selected_row] = get_config_data()->to;
  save_preset_data();
}

/*
 * Hide the menu (destroy)
 */
static void hide_preset_menu() {
  window_stack_remove(window, true);
  window_destroy(window);
}

/*
 * Here we capture when a user selects a menu item
 */
static void menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  // Use the row to specify which item will receive the select action
  selected_row = cell_index->row;
  hide_preset_menu();
  app_timer_register(MENU_ACTION_MS, do_menu_action, NULL);
}

/*
 * Here we capture when a user selects a menu item
 */
static void menu_select_long_click(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  // Use the row to specify which item will receive the select action
  selected_row = cell_index->row;
  hide_preset_menu();
  app_timer_register(MENU_ACTION_MS, do_long_menu_action, NULL);
}

/*
 * This initializes the menu upon window load
 */
static void window_load(Window *window) {

  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_frame(window_layer);
  centre = bounds.size.w / 2;
  width = bounds.size.w;

  menu_layer = menu_layer_create(bounds);
  
  #ifdef PBL_ROUND
  menu_layer_set_center_focused(menu_layer, true);
  #endif

  menu_layer_set_callbacks(menu_layer, NULL, (MenuLayerCallbacks ) 
                           { .get_num_sections = menu_get_num_sections_callback, 
                            .get_num_rows = menu_get_num_rows_callback, 
                            .get_header_height = menu_get_header_height_callback, 
                            .draw_header = menu_draw_header_callback, 
                            .draw_row = menu_draw_row_callback, 
                            .select_click = menu_select_callback,
                            .select_long_click = menu_select_long_click,});

  menu_layer_set_click_config_onto_window(menu_layer, window);

  layer_add_child(window_layer, menu_layer_get_layer_jf(menu_layer));
  
  menu_layer_set_normal_colors(menu_layer, MENU_BACKGROUND_COLOR, MENU_TEXT_COLOR);
  menu_layer_set_highlight_colors(menu_layer, MENU_HIGHLIGHT_BACKGROUND_COLOR, MENU_TEXT_COLOR);
  
}

/*
 * Unload the menu window
 */
static void window_unload(Window *window) {
  menu_layer_destroy(menu_layer);
}

/*
 * Show the menu
 */
EXTFN void show_preset_menu() {
  window = window_create();
  
  read_preset_data();
  
  // Setup the window handlers
  window_set_window_handlers(window, (WindowHandlers ) { .load = window_load, .unload = window_unload, });
  window_stack_push(window, true /* Animated */);
}

/*
 * Set the preset to number picked 0 = early, 1 = medium, 2 = late
 */
EXTFN void set_using_preset(uint8_t eml) {
  
  // Ensure we've loaded the preset data (should we need to)
  if (preset_data.preset_ver != PRESET_VER) {
    read_preset_data();
  }
  
  // Load a sort array with the to field and the index number
  for (uint8_t i = 0; i < NO_PRESETS; i++) {
    sort_to[i] = preset_data.to[i]; 
    sort_no[i] = i;
  }
  
  // Perform a bubble sort
  uint32_t temp_to;
  uint8_t temp_no;
  for (uint8_t write = 0; write < NO_PRESETS; write++) {
    for (uint8_t sort = 0; sort < NO_PRESETS - 1; sort++) {
        if (sort_to[sort] > sort_to[sort + 1]) {
            temp_to = sort_to[sort + 1];
            sort_to[sort + 1] = sort_to[sort];
            sort_to[sort] = temp_to;
            temp_no = sort_no[sort + 1];
            sort_no[sort + 1] = sort_no[sort];
            sort_no[sort] = temp_no;
        }
    }
  }
  
  // Pick the number - element 0 will contain the index to the earliest, 1 to the medium and 2 to the latest
  uint8_t no = sort_no[eml];
  
  // Set the preset and run away
  get_config_data()->fromhr = preset_data.fromhr[no];
  get_config_data()->frommin = preset_data.frommin[no];
  get_config_data()->tohr = preset_data.tohr[no];
  get_config_data()->tomin = preset_data.tomin[no];
  get_config_data()->from = preset_data.from[no];
  get_config_data()->to = preset_data.to[no];    
  get_config_data()->smart = true;
  trigger_config_save();
  set_smart_status();
}

#endif




