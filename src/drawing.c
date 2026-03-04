/*
 * MimamoriKanshi System Monitor - Drawing Implementation
 *
 * Renders the monitoring widget using Cairo.  Each of the 6 rows has
 * its own circular-buffer image surface so that a full redraw on
 * every tick is unnecessary: only one vertical pixel-column is painted
 * into the surface per update, and the surface is composited into the
 * widget with the correct offset to produce a scrolling appearance.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "plugin.h"
#include "drawing.h"

/* ── Geometry helpers ────────────────────────────────────────────── */

/* Width of the scrolling history area (pixels) */
static gint
history_width(const MimamorikanshiConfig *cfg)
{
    gint content_w = cfg->width_px - 2 * cfg->border_px;
    gint hw = content_w - cfg->latest_value_px;
    return hw > 0 ? hw : 0;
}

/* Y-offset of row `i` inside the widget (below the top border) */
static gint
row_y(const MimamorikanshiConfig *cfg, gint i)
{
    return cfg->border_px + i * (cfg->height_px + cfg->separator_px);
}

/* Color for a given row */
static const GdkRGBA *
row_color(const MimamorikanshiConfig *cfg, gint row)
{
    switch (row) {
    case ROW_CPU:          return &cfg->cpu_color;
    case ROW_MEMORY:       return &cfg->memory_color;
    case ROW_DISK_READ:    return &cfg->disk_read_color;
    case ROW_DISK_WRITE:   return &cfg->disk_write_color;
    case ROW_NET_DOWNLOAD: return &cfg->network_download_color;
    case ROW_NET_UPLOAD:   return &cfg->network_upload_color;
    default:               return &cfg->cpu_color;
    }
}

/* ── Surface management ──────────────────────────────────────────── */

void
mimamorikanshi_drawing_ensure_surfaces(MimamorikanshiPlugin *mmk)
{
    const MimamorikanshiConfig *cfg = &mmk->config;
    gint hw = history_width(cfg);
    gint rh = cfg->height_px;

    if (hw <= 0 || rh <= 0) {
        mimamorikanshi_drawing_free_surfaces(mmk);
        return;
    }

    /* Skip if surfaces already match the required size */
    if (mmk->surface_width == hw && mmk->surface_height == rh &&
        mmk->row_surfaces[0] != NULL)
        return;

    /* Destroy old surfaces */
    mimamorikanshi_drawing_free_surfaces(mmk);

    mmk->surface_width  = hw;
    mmk->surface_height = rh;

    for (gint i = 0; i < NUM_ROWS; i++) {
        mmk->row_surfaces[i] = cairo_image_surface_create(
                                    CAIRO_FORMAT_ARGB32, hw, rh);
        if (cairo_surface_status(mmk->row_surfaces[i]) != CAIRO_STATUS_SUCCESS) {
            g_warning("mimamorikanshi: failed to allocate surface for row %d", i);
            cairo_surface_destroy(mmk->row_surfaces[i]);
            mmk->row_surfaces[i] = NULL;
        }
        mmk->write_index[i] = 0;

        /* Fill new surface with background colour */
        if (mmk->row_surfaces[i]) {
            cairo_t *scr = cairo_create(mmk->row_surfaces[i]);
            cairo_set_operator(scr, CAIRO_OPERATOR_SOURCE);
            gdk_cairo_set_source_rgba(scr, &cfg->background_color);
            cairo_paint(scr);
            cairo_destroy(scr);
        }
    }
}

void
mimamorikanshi_drawing_free_surfaces(MimamorikanshiPlugin *mmk)
{
    for (gint i = 0; i < NUM_ROWS; i++) {
        if (mmk->row_surfaces[i]) {
            cairo_surface_destroy(mmk->row_surfaces[i]);
            mmk->row_surfaces[i] = NULL;
        }
        mmk->write_index[i] = 0;
    }
    mmk->surface_width  = 0;
    mmk->surface_height = 0;
}

/* ── Column update (called per tick) ─────────────────────────────── */

void
mimamorikanshi_drawing_update_columns(MimamorikanshiPlugin *mmk)
{
    const MimamorikanshiConfig *cfg = &mmk->config;
    gint hw = mmk->surface_width;
    gint rh = mmk->surface_height;

    if (hw <= 0 || rh <= 0)
        return;

    for (gint i = 0; i < NUM_ROWS; i++) {
        cairo_surface_t *surf = mmk->row_surfaces[i];
        if (!surf)
            continue;

        gint wi = mmk->write_index[i];
        gdouble val = mmk->monitor.values[i];
        const GdkRGBA *col = row_color(cfg, i);

        cairo_t *scr = cairo_create(surf);
        cairo_set_operator(scr, CAIRO_OPERATOR_SOURCE);

        /* Clear column with background */
        gdk_cairo_set_source_rgba(scr, &cfg->background_color);
        cairo_rectangle(scr, wi, 0, 1, rh);
        cairo_fill(scr);

        /* Draw value bar from bottom */
        gdouble bar_h = val * (gdouble)rh;
        if (bar_h > 0.0) {
            gdk_cairo_set_source_rgba(scr, col);
            cairo_rectangle(scr, wi, rh - bar_h, 1, bar_h);
            cairo_fill(scr);
        }

        cairo_destroy(scr);

        mmk->write_index[i] = (wi + 1) % hw;
    }
}

/* ── Fallback full-redraw (no surfaces) ──────────────────────────── */

static void
draw_row_fallback(cairo_t *cr, const MimamorikanshiConfig *cfg,
                  gint row G_GNUC_UNUSED, gint x, gint y)
{
    /* Just fill row with background – no history available */
    gdk_cairo_set_source_rgba(cr, &cfg->background_color);
    cairo_rectangle(cr, x, y,
                    cfg->width_px - 2 * cfg->border_px, cfg->height_px);
    cairo_fill(cr);
}

/* ── Main draw callback ──────────────────────────────────────────── */

gboolean
mimamorikanshi_drawing_draw(GtkWidget *widget G_GNUC_UNUSED,
                             cairo_t   *cr,
                             gpointer   user_data)
{
    MimamorikanshiPlugin *mmk = user_data;
    const MimamorikanshiConfig *cfg = &mmk->config;

    gint total_w = cfg->width_px;
    gint total_h = mimamorikanshi_calc_widget_height(cfg);
    gint hw      = history_width(cfg);
    gint rh      = cfg->height_px;
    gint bpx     = cfg->border_px;
    gint lvpx    = cfg->latest_value_px;
    gint content_x = bpx;

    /* ── Background fill ── */
    gdk_cairo_set_source_rgba(cr, &cfg->background_color);
    cairo_rectangle(cr, 0, 0, total_w, total_h);
    cairo_fill(cr);

    /* ── Border ── */
    if (bpx > 0) {
        gdk_cairo_set_source_rgba(cr, &cfg->border_color);
        /* Top */
        cairo_rectangle(cr, 0, 0, total_w, bpx);
        cairo_fill(cr);
        /* Bottom */
        cairo_rectangle(cr, 0, total_h - bpx, total_w, bpx);
        cairo_fill(cr);
        /* Left */
        cairo_rectangle(cr, 0, 0, bpx, total_h);
        cairo_fill(cr);
        /* Right */
        cairo_rectangle(cr, total_w - bpx, 0, bpx, total_h);
        cairo_fill(cr);
    }

    /* ── Rows ── */
    for (gint i = 0; i < NUM_ROWS; i++) {
        gint ry = row_y(cfg, i);

        /* ── Separator above (except first row) ── */
        if (i > 0 && cfg->separator_px > 0) {
            gdk_cairo_set_source_rgba(cr, &cfg->separator_color);
            cairo_rectangle(cr, content_x, ry - cfg->separator_px,
                            total_w - 2 * bpx, cfg->separator_px);
            cairo_fill(cr);
        }

        /* ── History graph (circular-buffer composite) ── */
        cairo_surface_t *surf = mmk->row_surfaces[i];
        if (surf && hw > 0) {
            gint wi = mmk->write_index[i];

            cairo_save(cr);
            cairo_rectangle(cr, content_x, ry, hw, rh);
            cairo_clip(cr);

            /* Part 1: older data [wi .. end] → left side of display */
            gint older_len = hw - wi;
            if (older_len > 0) {
                cairo_set_source_surface(cr, surf,
                                         content_x - wi, ry);
                cairo_rectangle(cr, content_x, ry, older_len, rh);
                cairo_fill(cr);
            }

            /* Part 2: newer data [0 .. wi-1] → right side of display */
            if (wi > 0) {
                cairo_set_source_surface(cr, surf,
                                         content_x + older_len, ry);
                cairo_rectangle(cr, content_x + older_len, ry, wi, rh);
                cairo_fill(cr);
            }

            cairo_restore(cr);
        } else {
            draw_row_fallback(cr, cfg, i, content_x, ry);
        }

        /* ── Latest-value bar ── */
        if (lvpx > 0) {
            gint lv_x = content_x + hw;
            gdouble val = mmk->monitor.values[i];
            const GdkRGBA *col = row_color(cfg, i);

            /* Background behind latest-value bar */
            gdk_cairo_set_source_rgba(cr, &cfg->background_color);
            cairo_rectangle(cr, lv_x, ry, lvpx, rh);
            cairo_fill(cr);

            /* Bar from bottom */
            gdouble bar_h = val * (gdouble)rh;
            if (bar_h > 0.0) {
                gdk_cairo_set_source_rgba(cr, col);
                cairo_rectangle(cr, lv_x, ry + rh - bar_h, lvpx, bar_h);
                cairo_fill(cr);
            }
        }

        /* ── Text overlay ── */
        {
            gchar text[32];
            gdouble rawv = mmk->monitor.raw_values[i];
            snprintf(text, sizeof(text), "%.0f", round(rawv));

            cairo_save(cr);
            cairo_rectangle(cr, content_x, ry,
                            total_w - 2 * bpx, rh);
            cairo_clip(cr);

            cairo_select_font_face(cr,
                cfg->text_font_family ? cfg->text_font_family : "monospace",
                CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
            cairo_set_font_size(cr, cfg->text_font_size);

            cairo_font_extents_t fe;
            cairo_font_extents(cr, &fe);

            gdouble tx = content_x + cfg->text_left_padding_px;
            /* Vertically centre the text in the row */
            gdouble ty = ry + (rh + fe.ascent - fe.descent) / 2.0;

            gdk_cairo_set_source_rgba(cr, &cfg->text_color);
            cairo_move_to(cr, tx, ty);
            cairo_show_text(cr, text);

            cairo_restore(cr);
        }
    }

    return FALSE;
}
