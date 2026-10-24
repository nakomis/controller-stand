#include "display.h"

#include <string.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lvgl_port.h"
#include "pins.h"

#define LCD_HOST  SPI2_HOST
#define LCD_RES   240
#define LCD_HZ    (40 * 1000 * 1000)

#define BL_FULL 255
#define BL_DIM  24

static const char *TAG = "display";

static const char *const BAY_NAMES[BAY_COUNT] = {"Front", "Back"};

typedef struct {
    lv_obj_t *arc;
    lv_obj_t *name;
    lv_obj_t *text;
} bay_widgets_t;

static bay_widgets_t s_w[BAY_COUNT];

static lv_color_t arc_colour(const bay_view_t *v)
{
    if (v->fault) {
        return lv_color_hex(0xE5484D);
    }
    switch (v->state) {
    case CE_FULL:
        return lv_color_hex(0x30A46C);
    case CE_CHARGING:
    case CE_SETTLING:
        return lv_color_hex(0x12A594);
    case CE_EMPTY:
    default:
        return lv_color_hex(0x3A3F4B);
    }
}

static void backlight_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);
    ledc_channel_config_t ch = {
        .gpio_num = PIN_LCD_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = BL_FULL,
    };
    ledc_channel_config(&ch);
}

void display_set_dim(bool dim)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, dim ? BL_DIM : BL_FULL);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static lv_obj_t *make_arc(lv_obj_t *scr, int size, int width)
{
    lv_obj_t *arc = lv_arc_create(scr);
    lv_obj_set_size(arc, size, size);
    lv_obj_center(arc);
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, 0);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(arc, width, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x1C1F26), LV_PART_MAIN);
    return arc;
}

static lv_obj_t *make_label(lv_obj_t *scr, const lv_font_t *font, lv_color_t colour, int y)
{
    lv_obj_t *l = lv_label_create(scr);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, colour, 0);
    lv_obj_align(l, LV_ALIGN_CENTER, 0, y);
    lv_label_set_text(l, "");
    return l;
}

static void build_ui(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    s_w[BAY_FRONT].arc = make_arc(scr, 236, 12);
    s_w[BAY_BACK].arc = make_arc(scr, 206, 12);

    lv_color_t dim = lv_color_hex(0x8B8D98);
    lv_color_t bright = lv_color_white();
    s_w[BAY_FRONT].name = make_label(scr, &lv_font_montserrat_14, dim, -44);
    s_w[BAY_FRONT].text = make_label(scr, &lv_font_montserrat_20, bright, -20);
    s_w[BAY_BACK].name = make_label(scr, &lv_font_montserrat_14, dim, 18);
    s_w[BAY_BACK].text = make_label(scr, &lv_font_montserrat_20, bright, 42);
    for (int i = 0; i < BAY_COUNT; i++) {
        lv_label_set_text(s_w[i].name, BAY_NAMES[i]);
    }
}

esp_err_t display_init(void)
{
    backlight_init();

    spi_bus_config_t bus = {
        .sclk_io_num = PIN_LCD_SCLK,
        .mosi_io_num = PIN_LCD_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_RES * 40 * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO), TAG, "spi bus");

    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = PIN_LCD_DC,
        .cs_gpio_num = PIN_LCD_CS,
        .pclk_hz = LCD_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &io), TAG,
                        "panel io");

    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_gc9a01(io, &panel_cfg, &panel), TAG, "gc9a01");
    esp_lcd_panel_reset(panel);
    esp_lcd_panel_init(panel);
    esp_lcd_panel_invert_color(panel, true);
    esp_lcd_panel_disp_on_off(panel, true);

    const lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_RETURN_ON_ERROR(lvgl_port_init(&port_cfg), TAG, "lvgl port");

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io,
        .panel_handle = panel,
        .buffer_size = LCD_RES * 40,
        .double_buffer = true,
        .hres = LCD_RES,
        .vres = LCD_RES,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {.swap_xy = false, .mirror_x = true, .mirror_y = false},
        .flags = {.buff_dma = true, .swap_bytes = true},
    };
    if (lvgl_port_add_disp(&disp_cfg) == NULL) {
        return ESP_FAIL;
    }

    lvgl_port_lock(0);
    build_ui();
    lvgl_port_unlock();
    return ESP_OK;
}

void display_update(const bay_view_t views[BAY_COUNT])
{
    lvgl_port_lock(0);
    for (int i = 0; i < BAY_COUNT; i++) {
        const bay_view_t *v = &views[i];
        int value = v->state == CE_CHARGING || v->state == CE_FULL ? (int)(v->pct + 0.5f) : 0;
        lv_arc_set_value(s_w[i].arc, value);
        lv_obj_set_style_arc_color(s_w[i].arc, arc_colour(v), LV_PART_INDICATOR);
        lv_label_set_text(s_w[i].text, v->fault ? "fault" : v->text);
    }
    lvgl_port_unlock();
}
