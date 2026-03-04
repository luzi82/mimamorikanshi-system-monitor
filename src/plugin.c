/*
 * MimamoriKanshi System Monitor - Plugin Implementation
 *
 * Entry point for the Xfce panel plugin.  Handles lifecycle
 * (construct / free), the periodic polling timer, and wiring
 * everything together.
 */

#include <string.h>
#include "plugin.h"

/* ── Geometry helpers ────────────────────────────────────────────── */

gint
mimamorikanshi_calc_widget_height(const MimamorikanshiConfig *cfg)
{
    /* border_top + (6 × row_height) + (5 × separator) + border_bottom */
    return 2 * cfg->border_px
         + NUM_ROWS * cfg->height_px
         + (NUM_ROWS - 1) * cfg->separator_px;
}

gint
mimamorikanshi_calc_history_width(const MimamorikanshiConfig *cfg)
{
    gint content_w = cfg->width_px - 2 * cfg->border_px;
    gint hw = content_w - cfg->latest_value_px;
    return hw > 0 ? hw : 0;
}

/* ── Surface management ──────────────────────────────────────────── */

void
mimamorikanshi_recreate_surfaces(MimamorikanshiPlugin *mmk)
{
    mimamorikanshi_drawing_ensure_surfaces(mmk);
}

/* ── Timer ───────────────────────────────────────────────────────── */

static gboolean
timer_tick(gpointer data)
{
    MimamorikanshiPlugin *mmk = data;

    /* Let suspend module decide whether we should skip this tick */
    if (mimamorikanshi_suspend_tick(mmk))
        return G_SOURCE_REMOVE; /* timer is stopped by do_suspend */

    /* Collect metrics */
    mimamorikanshi_monitor_update(mmk);

    /* Only update the graph if the baseline was already valid
       (i.e., the first sample was used to record deltas). */
    if (mmk->monitor.baseline_valid) {
        mimamorikanshi_drawing_update_columns(mmk);
        if (mmk->drawing_area)
            gtk_widget_queue_draw(mmk->drawing_area);
    }

    return G_SOURCE_CONTINUE;
}

void
mimamorikanshi_stop_timer(MimamorikanshiPlugin *mmk)
{
    if (mmk->timer_id > 0) {
        g_source_remove(mmk->timer_id);
        mmk->timer_id = 0;
    }
}

void
mimamorikanshi_start_timer(MimamorikanshiPlugin *mmk)
{
    mimamorikanshi_stop_timer(mmk);

    guint interval = (guint)mmk->config.update_interval_ms;
    if (interval < 50)
        interval = 50;

    mmk->timer_id = g_timeout_add(interval, timer_tick, mmk);
}

void
mimamorikanshi_restart_timer(MimamorikanshiPlugin *mmk)
{
    mimamorikanshi_start_timer(mmk);
}

/* ── Panel plugin signal handlers ────────────────────────────────── */

static void
on_free_data(XfcePanelPlugin *plugin G_GNUC_UNUSED, gpointer data)
{
    MimamorikanshiPlugin *mmk = data;

    mimamorikanshi_stop_timer(mmk);
    mimamorikanshi_suspend_free(mmk);
    mimamorikanshi_config_disconnect_signals(mmk);
    mimamorikanshi_drawing_free_surfaces(mmk);
    mimamorikanshi_monitor_free(&mmk->monitor);
    mimamorikanshi_config_free(&mmk->config);
    g_free(mmk->property_base);
    g_free(mmk);
}

static gboolean
on_size_changed(XfcePanelPlugin *plugin G_GNUC_UNUSED,
                gint             size   G_GNUC_UNUSED,
                gpointer         data)
{
    MimamorikanshiPlugin *mmk = data;
    gint h = mimamorikanshi_calc_widget_height(&mmk->config);
    gtk_widget_set_size_request(mmk->drawing_area,
                                mmk->config.width_px, h);
    return TRUE;
}

static void
on_configure_plugin(XfcePanelPlugin *plugin G_GNUC_UNUSED, gpointer data)
{
    MimamorikanshiPlugin *mmk = data;
    mimamorikanshi_dialog_show(mmk);
}

static void
on_about(XfcePanelPlugin *plugin G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
    const gchar *authors[] = { "MimamoriKanshi contributors", NULL };
    gtk_show_about_dialog(NULL,
        "program-name", "MimamoriKanshi System Monitor",
        "version",      "0.1.0",
        "comments",     "A lightweight Xfce panel plugin for monitoring "
                        "CPU, memory, disk, and network usage.",
        "authors",      authors,
        "license-type", GTK_LICENSE_GPL_2_0,
        NULL);
}

/* ── Plugin construction (entry point) ───────────────────────────── */

static void
mimamorikanshi_construct(XfcePanelPlugin *plugin)
{
    /* ── Allocate plugin struct ── */
    MimamorikanshiPlugin *mmk = g_new0(MimamorikanshiPlugin, 1);
    mmk->panel_plugin = plugin;

    /* ── Xfconf ── */
    xfconf_init(NULL);
    mmk->channel       = xfconf_channel_get("xfce4-panel");
    mmk->property_base = g_strdup(
        xfce_panel_plugin_get_property_base(plugin));

    /* ── Configuration ── */
    mimamorikanshi_config_init_defaults(&mmk->config);
    mimamorikanshi_config_load(mmk);
    mimamorikanshi_config_connect_signals(mmk);

    /* ── Monitor ── */
    mimamorikanshi_monitor_init(&mmk->monitor);
    mimamorikanshi_monitor_refresh_sector_sizes(
        &mmk->monitor, mmk->config.disks, mmk->config.n_disks);

    /* ── Drawing area ── */
    mmk->drawing_area = gtk_drawing_area_new();
    gint h = mimamorikanshi_calc_widget_height(&mmk->config);
    gtk_widget_set_size_request(mmk->drawing_area,
                                mmk->config.width_px, h);
    g_signal_connect(mmk->drawing_area, "draw",
                     G_CALLBACK(mimamorikanshi_drawing_draw), mmk);

    gtk_container_add(GTK_CONTAINER(plugin), mmk->drawing_area);
    gtk_widget_show(mmk->drawing_area);

    /* ── Create Cairo surfaces ── */
    mimamorikanshi_drawing_ensure_surfaces(mmk);

    /* ── Suspend / resume ── */
    mimamorikanshi_suspend_init(mmk);
    g_signal_connect(mmk->drawing_area, "map",
                     G_CALLBACK(mimamorikanshi_suspend_on_map), mmk);
    g_signal_connect(mmk->drawing_area, "unmap",
                     G_CALLBACK(mimamorikanshi_suspend_on_unmap), mmk);

    /* ── Panel plugin signals ── */
    g_signal_connect(plugin, "free-data",
                     G_CALLBACK(on_free_data), mmk);
    g_signal_connect(plugin, "size-changed",
                     G_CALLBACK(on_size_changed), mmk);
    g_signal_connect(plugin, "configure-plugin",
                     G_CALLBACK(on_configure_plugin), mmk);
    g_signal_connect(plugin, "about",
                     G_CALLBACK(on_about), mmk);

    xfce_panel_plugin_menu_show_configure(plugin);
    xfce_panel_plugin_menu_show_about(plugin);
    xfce_panel_plugin_set_small(plugin, TRUE);

    /* ── Start timer ── */
    mimamorikanshi_start_timer(mmk);
}

XFCE_PANEL_PLUGIN_REGISTER(mimamorikanshi_construct)
