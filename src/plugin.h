/*
 * MimamoriKanshi System Monitor - Plugin Header
 *
 * Central data structure aggregating configuration, monitoring state,
 * drawing surfaces, and suspend state.
 */

#ifndef MIMAMORIKANSHI_PLUGIN_H
#define MIMAMORIKANSHI_PLUGIN_H

#include <gtk/gtk.h>
#include <libxfce4panel/libxfce4panel.h>
#include <xfconf/xfconf.h>
#include <cairo/cairo.h>

#include "common.h"
#include "config.h"
#include "monitor.h"
#include "suspend.h"
#include "drawing.h"
#include "dialog.h"

typedef struct _MimamorikanshiPlugin MimamorikanshiPlugin;

struct _MimamorikanshiPlugin {
    XfcePanelPlugin *panel_plugin;

    /* Configuration */
    MimamorikanshiConfig config;
    XfconfChannel *channel;
    gchar *property_base;

    /* Monitoring */
    MonitorState monitor;

    /* Drawing – per-row circular-buffer surfaces */
    GtkWidget *drawing_area;
    cairo_surface_t *row_surfaces[NUM_ROWS];
    gint write_index[NUM_ROWS];
    gint surface_width;   /* current history-graph width */
    gint surface_height;  /* current row height */

    /* Regular polling timer */
    guint timer_id;

    /* Suspend / resume */
    SuspendState suspend;

    /* Set to TRUE once the very first valid sample has been emitted */
    gboolean first_sample_discarded;
};

/* Calculate total widget height from config */
gint mimamorikanshi_calc_widget_height(const MimamorikanshiConfig *cfg);

/* Calculate history graph width (scrolling area, excluding latest-value
   bar and borders) */
gint mimamorikanshi_calc_history_width(const MimamorikanshiConfig *cfg);

/* Destroy and recreate per-row surfaces to match current config */
void mimamorikanshi_recreate_surfaces(MimamorikanshiPlugin *mmk);

/* Stop the regular polling timer */
void mimamorikanshi_stop_timer(MimamorikanshiPlugin *mmk);

/* Start (or restart) the regular polling timer */
void mimamorikanshi_start_timer(MimamorikanshiPlugin *mmk);

/* Restart the timer (stop + start) */
void mimamorikanshi_restart_timer(MimamorikanshiPlugin *mmk);

#endif /* MIMAMORIKANSHI_PLUGIN_H */
