#include "mgos.h"
#include "led_master.h"

typedef struct {
    uint8_t rings;
    uint32_t num_ringpix;
    uint32_t num_pix;
    tools_rgb_data *ring_colors;
    tools_rgb_data black;
    audio_trigger_data* atd;    
} spiral_data;

static spiral_data *spd;

static void mgos_intern_spiral_flash_calc_colors(mgos_rgbleds* leds, spiral_data *spd)
{
   for (int i = 0; i < leds->panel_height; i++) {
        double h = (i * 1.0 / (leds->panel_height - 1)) * 360.0;
        double s = mgos_sys_config_get_ledeffects_spiral_flash_saturation();
        double v = mgos_sys_config_get_ledeffects_spiral_flash_value();
        spd->ring_colors[i] = tools_hsv_to_rgb(h, s, v);
        LOG(LL_VERBOSE_DEBUG, ("mgos_intern_spiral_flash:\tH: %.02f\tS: %.02f\tV: %.02f, #%.02X%.02X%.02X", h, s, v, spd->ring_colors[i].r, spd->ring_colors[i].g, spd->ring_colors[i].b));
    }
    memset(&spd->black, 0, sizeof(tools_rgb_data));
}

static void mgos_intern_spiral_flash_init(mgos_rgbleds* leds)
{
    leds->timeout = mgos_sys_config_get_ledeffects_spiral_flash_timeout();
    leds->dim_all = mgos_sys_config_get_ledeffects_spiral_flash_dim_all();

    spd = calloc(1, sizeof(spiral_data));
    spd->atd = (audio_trigger_data*)leds->audio_data;
    spd->num_pix = leds->panel_width * leds->panel_height;
    spd->rings = mgos_sys_config_get_ledeffects_spiral_flash_rings();
    spd->num_ringpix = spd->rings * leds->panel_width;
    spd->ring_colors = calloc(leds->panel_height * 4, sizeof(tools_rgb_data));
    
    spd->atd->level = 1.0;
    spd->atd->level_average = 1.0;

    mgos_intern_spiral_flash_calc_colors(leds, spd);
}

static void mgos_intern_spiral_flash_exit(mgos_rgbleds* leds)
{
    if (spd->ring_colors != NULL) {
        free(spd->ring_colors);
    }

    if (spd != NULL) {
        free(spd);
        spd = NULL;
    }
}

static void mgos_intern_spiral_flash_loop(mgos_rgbleds* leds)
{
    uint32_t num_rows = leds->panel_height;
    uint32_t num_cols = leds->panel_width;
 
    tools_rgb_data out_pix;
    int run = leds->internal_loops;
    run = run <= 0 ? 0 : run;

    while (run--) {
        int loops = spd->rings;
        while (loops--) {
            bool do_run = true;
            mgos_universal_led_clear(leds);
            for (int row = 0; row < num_rows; row++) {
                for (int col = 0; col < num_cols; col++) {
//                    int color_pos = (((row + leds->pix_pos) * num_cols) + col) % spd->num_pix;
//                    do_run = ((row * num_cols) + col >= leds->pix_pos) && ((row * num_cols) + col < (leds->pix_pos + spd->num_ringpix));
                    do_run = row >= leds->pix_pos && row < (leds->pix_pos + (int) round(spd->rings * 2.0 * spd->atd->level));
                    if (do_run) {
                        int col_pos = min((num_rows * 4) - 1, (int)round(num_rows * spd->atd->level * 4.0));
                        out_pix = spd->ring_colors[col_pos];
                        //LOG(LL_INFO, ("mgos_intern_spiral_flash_loop:\tPos: %d\tX: %d\tY: %d\tR: 0x%.02X\tG: 0x%.02X\tB: 0x%.02X", leds->pix_pos, col, row, out_pix.r, out_pix.g, out_pix.b));
                        mgos_universal_led_plot_pixel(leds, col, num_rows - 1 - row, out_pix, false);
                        mgos_wdt_feed();
                    }
                }
            }
            mgos_universal_led_show(leds);
            mgos_msleep(mgos_sys_config_get_ledeffects_spiral_flash_sleep());
            leds->pix_pos = (leds->pix_pos + 1) % num_rows;
        }
    }
}

void mgos_ledeffects_spiral_flash(void* param, mgos_rgbleds_action action)
{
    static bool do_time = false;
    static uint32_t max_time = 0;
    uint32_t time = (mgos_uptime_micros() / 1000);
    mgos_rgbleds* leds = (mgos_rgbleds*)param;

    switch (action) {
    case MGOS_RGBLEDS_ACT_INIT:
        LOG(LL_INFO, ("mgos_ledeffects_spiral_flash: called (init)"));
        mgos_intern_spiral_flash_init(leds);
        break;
    case MGOS_RGBLEDS_ACT_EXIT:
        LOG(LL_INFO, ("mgos_ledeffects_spiral_flash: called (exit)"));
        mgos_intern_spiral_flash_exit(leds);
        break;
    case MGOS_RGBLEDS_ACT_LOOP:
        mgos_intern_spiral_flash_loop(leds);
        if (do_time) {
            time = (mgos_uptime_micros() /1000) - time;
            max_time = (time > max_time) ? time : max_time;
            LOG(LL_VERBOSE_DEBUG, ("Spiral flash loop duration: %d milliseconds, max: %d ...", time / 1000, max_time / 1000));
        }
        break;
    }
}

bool mgos_ledeffects_spiral_flash_init(void) { 
  LOG(LL_INFO, ("mgos_spiral_flash_init ..."));
  ledmaster_add_effect("ANIM_SPIRAL_FLASH", mgos_ledeffects_spiral_flash);
  return true;
}
