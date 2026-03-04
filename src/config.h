/*
 * MimamoriKanshi System Monitor - Configuration Header
 *
 * Defines the plugin configuration structure and Xfconf management functions.
 */

#ifndef MIMAMORIKANSHI_CONFIG_H
#define MIMAMORIKANSHI_CONFIG_H

#include <glib.h>
#include <gdk/gdk.h>
#include <xfconf/xfconf.h>

typedef struct _MimamorikanshiConfig MimamorikanshiConfig;

struct _MimamorikanshiConfig {
    /* Dimensions */
    gint width_px;
    gint height_px;
    gint latest_value_px;

    /* Colors */
    GdkRGBA background_color;
    gint border_px;
    GdkRGBA border_color;
    gint separator_px;
    GdkRGBA separator_color;

    /* Text */
    GdkRGBA text_color;
    gint text_left_padding_px;
    gchar *text_font_family;
    gint text_font_size;

    /* CPU */
    GdkRGBA cpu_color;

    /* Memory */
    GdkRGBA memory_color;

    /* Disk */
    gchar **disks;
    gint n_disks;
    GdkRGBA disk_read_color;
    gint disk_read_max_mib_s;
    GdkRGBA disk_write_color;
    gint disk_write_max_mib_s;

    /* Network */
    gchar **networks;
    gint n_networks;
    GdkRGBA network_download_color;
    gint network_download_max_mbit_s;
    GdkRGBA network_upload_color;
    gint network_upload_max_mbit_s;

    /* Timing */
    gint update_interval_ms;
    gint suspend_after_ms;
    gint suspend_poll_ms;
};

/* Forward declaration */
typedef struct _MimamorikanshiPlugin MimamorikanshiPlugin;

/* Initialize config with default values */
void mimamorikanshi_config_init_defaults(MimamorikanshiConfig *config);

/* Load configuration from Xfconf */
void mimamorikanshi_config_load(MimamorikanshiPlugin *mmk);

/* Free allocated memory in config */
void mimamorikanshi_config_free(MimamorikanshiConfig *config);

/* Connect Xfconf property-changed signals */
void mimamorikanshi_config_connect_signals(MimamorikanshiPlugin *mmk);

/* Disconnect Xfconf signals */
void mimamorikanshi_config_disconnect_signals(MimamorikanshiPlugin *mmk);

/* Utility: parse "#RRGGBB" hex string to GdkRGBA */
gboolean mimamorikanshi_parse_hex_color(const gchar *hex, GdkRGBA *rgba);

/* Utility: convert GdkRGBA to "#RRGGBB" hex string (caller frees) */
gchar *mimamorikanshi_color_to_hex(const GdkRGBA *rgba);

#endif /* MIMAMORIKANSHI_CONFIG_H */
