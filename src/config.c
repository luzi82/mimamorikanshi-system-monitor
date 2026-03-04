/*
 * MimamoriKanshi System Monitor - Configuration Implementation
 *
 * Manages Xfconf property storage, loading, and change notification.
 */

#include <string.h>
#include <stdio.h>
#include "plugin.h"

/* ── Utility ─────────────────────────────────────────────────────── */

gboolean
mimamorikanshi_parse_hex_color(const gchar *hex, GdkRGBA *rgba)
{
    if (hex == NULL || hex[0] != '#' || strlen(hex) != 7)
        return FALSE;

    unsigned int r, g, b;
    if (sscanf(hex + 1, "%02x%02x%02x", &r, &g, &b) != 3)
        return FALSE;

    rgba->red   = r / 255.0;
    rgba->green = g / 255.0;
    rgba->blue  = b / 255.0;
    rgba->alpha = 1.0;
    return TRUE;
}

gchar *
mimamorikanshi_color_to_hex(const GdkRGBA *rgba)
{
    return g_strdup_printf("#%02X%02X%02X",
                           (unsigned int)(rgba->red   * 255),
                           (unsigned int)(rgba->green * 255),
                           (unsigned int)(rgba->blue  * 255));
}

/* ── Defaults ────────────────────────────────────────────────────── */

void
mimamorikanshi_config_init_defaults(MimamorikanshiConfig *cfg)
{
    memset(cfg, 0, sizeof(*cfg));

    cfg->width_px          = 26;
    cfg->height_px         = 10;
    cfg->latest_value_px   = 3;

    mimamorikanshi_parse_hex_color("#000000", &cfg->background_color);
    cfg->border_px = 1;
    mimamorikanshi_parse_hex_color("#404040", &cfg->border_color);
    cfg->separator_px = 1;
    mimamorikanshi_parse_hex_color("#404040", &cfg->separator_color);

    mimamorikanshi_parse_hex_color("#FFFFFF", &cfg->text_color);
    cfg->text_left_padding_px = 1;
    cfg->text_font_family     = g_strdup("DejaVu Sans Mono");
    cfg->text_font_size       = 8;

    mimamorikanshi_parse_hex_color("#00FF00", &cfg->cpu_color);
    mimamorikanshi_parse_hex_color("#FFFF00", &cfg->memory_color);

    cfg->disks   = g_new0(gchar *, 2);
    cfg->disks[0] = g_strdup("sda");
    cfg->n_disks  = 1;
    mimamorikanshi_parse_hex_color("#00FFFF", &cfg->disk_read_color);
    cfg->disk_read_max_mib_s = 100;
    mimamorikanshi_parse_hex_color("#FF00FF", &cfg->disk_write_color);
    cfg->disk_write_max_mib_s = 100;

    cfg->networks   = g_new0(gchar *, 2);
    cfg->networks[0] = g_strdup("eth0");
    cfg->n_networks  = 1;
    mimamorikanshi_parse_hex_color("#0080FF", &cfg->network_download_color);
    cfg->network_download_max_mbit_s = 100;
    mimamorikanshi_parse_hex_color("#FF8000", &cfg->network_upload_color);
    cfg->network_upload_max_mbit_s = 100;

    cfg->update_interval_ms = 500;
    cfg->suspend_after_ms   = 60000;
    cfg->suspend_poll_ms    = 5000;
}

/* ── Free ────────────────────────────────────────────────────────── */

void
mimamorikanshi_config_free(MimamorikanshiConfig *cfg)
{
    g_free(cfg->text_font_family);
    cfg->text_font_family = NULL;

    g_strfreev(cfg->disks);
    cfg->disks   = NULL;
    cfg->n_disks = 0;

    g_strfreev(cfg->networks);
    cfg->networks   = NULL;
    cfg->n_networks = 0;
}

/* ── Xfconf helpers ──────────────────────────────────────────────── */

static gchar *
prop_path(const MimamorikanshiPlugin *mmk, const gchar *name)
{
    return g_strconcat(mmk->property_base, "/", name, NULL);
}

static gint
read_int(const MimamorikanshiPlugin *mmk, const gchar *name, gint def)
{
    gchar *path = prop_path(mmk, name);
    gint val = xfconf_channel_get_int(mmk->channel, path, def);
    g_free(path);
    return val;
}

static void
read_color(const MimamorikanshiPlugin *mmk, const gchar *name,
           GdkRGBA *out, const GdkRGBA *def)
{
    gchar *path = prop_path(mmk, name);
    gchar *hex  = xfconf_channel_get_string(mmk->channel, path, NULL);
    g_free(path);

    if (hex == NULL || !mimamorikanshi_parse_hex_color(hex, out))
        *out = *def;
    g_free(hex);
}

static gchar *
read_string(const MimamorikanshiPlugin *mmk, const gchar *name,
            const gchar *def)
{
    gchar *path = prop_path(mmk, name);
    gchar *val  = xfconf_channel_get_string(mmk->channel, path, def);
    g_free(path);
    return val ? g_strdup(val) : g_strdup(def);
}

/* Read a string-list property. Returns a NULL-terminated gchar** and
   sets *out_count to the number of elements. */
static gchar **
read_string_list(const MimamorikanshiPlugin *mmk, const gchar *name,
                 gchar **defaults, gint def_count, gint *out_count)
{
    gchar *path = prop_path(mmk, name);
    gchar **list = xfconf_channel_get_string_list(mmk->channel, path);
    g_free(path);

    if (list != NULL) {
        *out_count = (gint)g_strv_length(list);
        return list;
    }

    /* Return a copy of defaults */
    gchar **copy = g_new0(gchar *, def_count + 1);
    for (gint i = 0; i < def_count; i++)
        copy[i] = g_strdup(defaults[i]);
    *out_count = def_count;
    return copy;
}

/* ── Load ────────────────────────────────────────────────────────── */

void
mimamorikanshi_config_load(MimamorikanshiPlugin *mmk)
{
    MimamorikanshiConfig *cfg = &mmk->config;

    /* Keep a copy of defaults for color fallback */
    MimamorikanshiConfig def;
    mimamorikanshi_config_init_defaults(&def);

    cfg->width_px        = read_int(mmk, "width-px",        def.width_px);
    cfg->height_px       = read_int(mmk, "height-px",       def.height_px);
    cfg->latest_value_px = read_int(mmk, "latest-value-px", def.latest_value_px);

    read_color(mmk, "background-color", &cfg->background_color, &def.background_color);
    cfg->border_px = read_int(mmk, "border-px", def.border_px);
    read_color(mmk, "border-color", &cfg->border_color, &def.border_color);
    cfg->separator_px = read_int(mmk, "separator-px", def.separator_px);
    read_color(mmk, "separator-color", &cfg->separator_color, &def.separator_color);

    read_color(mmk, "text-color", &cfg->text_color, &def.text_color);
    cfg->text_left_padding_px = read_int(mmk, "text-left-padding-px",
                                          def.text_left_padding_px);
    g_free(cfg->text_font_family);
    cfg->text_font_family = read_string(mmk, "text-font-family",
                                         def.text_font_family);
    cfg->text_font_size = read_int(mmk, "text-font-size", def.text_font_size);

    read_color(mmk, "cpu-color",    &cfg->cpu_color,    &def.cpu_color);
    read_color(mmk, "memory-color", &cfg->memory_color, &def.memory_color);

    g_strfreev(cfg->disks);
    cfg->disks = read_string_list(mmk, "disks", def.disks, def.n_disks,
                                   &cfg->n_disks);
    read_color(mmk, "disk-read-color",  &cfg->disk_read_color,  &def.disk_read_color);
    cfg->disk_read_max_mib_s  = read_int(mmk, "disk-read-max-mib-s",
                                          def.disk_read_max_mib_s);
    read_color(mmk, "disk-write-color", &cfg->disk_write_color, &def.disk_write_color);
    cfg->disk_write_max_mib_s = read_int(mmk, "disk-write-max-mib-s",
                                          def.disk_write_max_mib_s);

    g_strfreev(cfg->networks);
    cfg->networks = read_string_list(mmk, "networks", def.networks,
                                      def.n_networks, &cfg->n_networks);
    read_color(mmk, "network-download-color", &cfg->network_download_color,
               &def.network_download_color);
    cfg->network_download_max_mbit_s = read_int(mmk, "network-download-max-mbit-s",
                                                def.network_download_max_mbit_s);
    read_color(mmk, "network-upload-color", &cfg->network_upload_color,
               &def.network_upload_color);
    cfg->network_upload_max_mbit_s = read_int(mmk, "network-upload-max-mbit-s",
                                              def.network_upload_max_mbit_s);

    cfg->update_interval_ms = read_int(mmk, "update-interval-ms",
                                        def.update_interval_ms);
    cfg->suspend_after_ms   = read_int(mmk, "suspend-after-ms",
                                        def.suspend_after_ms);
    cfg->suspend_poll_ms    = read_int(mmk, "suspend-poll-ms",
                                        def.suspend_poll_ms);

    mimamorikanshi_config_free(&def);
}

/* ── Property-changed callback ───────────────────────────────────── */

static void
on_property_changed(XfconfChannel  *channel G_GNUC_UNUSED,
                    const gchar    *property,
                    const GValue   *value,
                    gpointer        user_data)
{
    MimamorikanshiPlugin *mmk = user_data;
    MimamorikanshiConfig *cfg = &mmk->config;

    /* Strip property base to get the short name */
    const gchar *name = property;
    if (g_str_has_prefix(property, mmk->property_base)) {
        name = property + strlen(mmk->property_base);
        if (*name == '/')
            name++;
    }

    gboolean need_resize = FALSE;
    gboolean need_redraw = FALSE;
    gboolean need_timer_restart = FALSE;
    gboolean need_disk_refresh = FALSE;
    gboolean need_net_refresh = FALSE;

    /* Integer properties */
    if (g_strcmp0(name, "width-px") == 0 && G_VALUE_HOLDS_INT(value)) {
        cfg->width_px = g_value_get_int(value);
        need_resize = TRUE;
    } else if (g_strcmp0(name, "height-px") == 0 && G_VALUE_HOLDS_INT(value)) {
        cfg->height_px = g_value_get_int(value);
        need_resize = TRUE;
    } else if (g_strcmp0(name, "latest-value-px") == 0 && G_VALUE_HOLDS_INT(value)) {
        cfg->latest_value_px = g_value_get_int(value);
        need_resize = TRUE;
    } else if (g_strcmp0(name, "border-px") == 0 && G_VALUE_HOLDS_INT(value)) {
        cfg->border_px = g_value_get_int(value);
        need_resize = TRUE;
    } else if (g_strcmp0(name, "separator-px") == 0 && G_VALUE_HOLDS_INT(value)) {
        cfg->separator_px = g_value_get_int(value);
        need_resize = TRUE;
    } else if (g_strcmp0(name, "text-left-padding-px") == 0 && G_VALUE_HOLDS_INT(value)) {
        cfg->text_left_padding_px = g_value_get_int(value);
        need_redraw = TRUE;
    } else if (g_strcmp0(name, "text-font-size") == 0 && G_VALUE_HOLDS_INT(value)) {
        cfg->text_font_size = g_value_get_int(value);
        need_redraw = TRUE;
    } else if (g_strcmp0(name, "disk-read-max-mib-s") == 0 && G_VALUE_HOLDS_INT(value)) {
        cfg->disk_read_max_mib_s = g_value_get_int(value);
        need_redraw = TRUE;
    } else if (g_strcmp0(name, "disk-write-max-mib-s") == 0 && G_VALUE_HOLDS_INT(value)) {
        cfg->disk_write_max_mib_s = g_value_get_int(value);
        need_redraw = TRUE;
    } else if (g_strcmp0(name, "network-download-max-mbit-s") == 0 && G_VALUE_HOLDS_INT(value)) {
        cfg->network_download_max_mbit_s = g_value_get_int(value);
        need_redraw = TRUE;
    } else if (g_strcmp0(name, "network-upload-max-mbit-s") == 0 && G_VALUE_HOLDS_INT(value)) {
        cfg->network_upload_max_mbit_s = g_value_get_int(value);
        need_redraw = TRUE;
    } else if (g_strcmp0(name, "update-interval-ms") == 0 && G_VALUE_HOLDS_INT(value)) {
        cfg->update_interval_ms = g_value_get_int(value);
        need_timer_restart = TRUE;
    } else if (g_strcmp0(name, "suspend-after-ms") == 0 && G_VALUE_HOLDS_INT(value)) {
        cfg->suspend_after_ms = g_value_get_int(value);
    } else if (g_strcmp0(name, "suspend-poll-ms") == 0 && G_VALUE_HOLDS_INT(value)) {
        cfg->suspend_poll_ms = g_value_get_int(value);
    }
    /* String properties */
    else if (g_strcmp0(name, "text-font-family") == 0 && G_VALUE_HOLDS_STRING(value)) {
        g_free(cfg->text_font_family);
        cfg->text_font_family = g_value_dup_string(value);
        need_redraw = TRUE;
    }
    /* Color properties (string) */
    else if (G_VALUE_HOLDS_STRING(value)) {
        const gchar *hex = g_value_get_string(value);
        GdkRGBA c;
        if (mimamorikanshi_parse_hex_color(hex, &c)) {
            if (g_strcmp0(name, "background-color") == 0)
                cfg->background_color = c;
            else if (g_strcmp0(name, "border-color") == 0)
                cfg->border_color = c;
            else if (g_strcmp0(name, "separator-color") == 0)
                cfg->separator_color = c;
            else if (g_strcmp0(name, "text-color") == 0)
                cfg->text_color = c;
            else if (g_strcmp0(name, "cpu-color") == 0)
                cfg->cpu_color = c;
            else if (g_strcmp0(name, "memory-color") == 0)
                cfg->memory_color = c;
            else if (g_strcmp0(name, "disk-read-color") == 0)
                cfg->disk_read_color = c;
            else if (g_strcmp0(name, "disk-write-color") == 0)
                cfg->disk_write_color = c;
            else if (g_strcmp0(name, "network-download-color") == 0)
                cfg->network_download_color = c;
            else if (g_strcmp0(name, "network-upload-color") == 0)
                cfg->network_upload_color = c;
            need_redraw = TRUE;
        }
    }

    /* Array properties are handled by re-reading from Xfconf */
    if (g_str_has_prefix(name, "disks")) {
        gchar *path = g_strconcat(mmk->property_base, "/disks", NULL);
        g_strfreev(cfg->disks);
        cfg->disks = xfconf_channel_get_string_list(mmk->channel, path);
        g_free(path);
        cfg->n_disks = cfg->disks ? (gint)g_strv_length(cfg->disks) : 0;
        need_disk_refresh = TRUE;
    }
    if (g_str_has_prefix(name, "networks")) {
        gchar *path = g_strconcat(mmk->property_base, "/networks", NULL);
        g_strfreev(cfg->networks);
        cfg->networks = xfconf_channel_get_string_list(mmk->channel, path);
        g_free(path);
        cfg->n_networks = cfg->networks ? (gint)g_strv_length(cfg->networks) : 0;
        need_net_refresh = TRUE;
    }

    /* Apply effects */
    if (need_disk_refresh) {
        mimamorikanshi_monitor_refresh_sector_sizes(&mmk->monitor,
                                                     cfg->disks, cfg->n_disks);
        mimamorikanshi_monitor_reset_baseline(&mmk->monitor);
    }
    if (need_net_refresh) {
        mimamorikanshi_monitor_reset_baseline(&mmk->monitor);
    }
    if (need_resize) {
        mimamorikanshi_recreate_surfaces(mmk);
        gint h = mimamorikanshi_calc_widget_height(cfg);
        gtk_widget_set_size_request(mmk->drawing_area, cfg->width_px, h);
        need_redraw = TRUE;
    }
    if (need_timer_restart) {
        mimamorikanshi_restart_timer(mmk);
    }
    if (need_redraw && mmk->drawing_area) {
        gtk_widget_queue_draw(mmk->drawing_area);
    }
}

/* ── Signal connection ───────────────────────────────────────────── */

void
mimamorikanshi_config_connect_signals(MimamorikanshiPlugin *mmk)
{
    g_signal_connect(mmk->channel, "property-changed",
                     G_CALLBACK(on_property_changed), mmk);
}

void
mimamorikanshi_config_disconnect_signals(MimamorikanshiPlugin *mmk)
{
    g_signal_handlers_disconnect_by_data(mmk->channel, mmk);
}
