/*
 * MimamoriKanshi System Monitor - Dialog Implementation
 *
 * A scrollable settings dialog following Xfce design guidelines.
 * Changes are written to Xfconf immediately; the property-changed
 * callback in config.c propagates them to the live plugin.
 */

#include <string.h>
#include <libxfce4ui/libxfce4ui.h>
#include "plugin.h"
#include "dialog.h"

/* ── Xfconf write helpers ────────────────────────────────────────── */

static gchar *
prop_path(const MimamorikanshiPlugin *mmk, const gchar *name)
{
    return g_strconcat(mmk->property_base, "/", name, NULL);
}

static void
write_int(MimamorikanshiPlugin *mmk, const gchar *name, gint val)
{
    gchar *path = prop_path(mmk, name);
    xfconf_channel_set_int(mmk->channel, path, val);
    g_free(path);
}

static void
write_color(MimamorikanshiPlugin *mmk, const gchar *name,
            const GdkRGBA *rgba)
{
    gchar *hex  = mimamorikanshi_color_to_hex(rgba);
    gchar *path = prop_path(mmk, name);
    xfconf_channel_set_string(mmk->channel, path, hex);
    g_free(path);
    g_free(hex);
}

static void
write_string(MimamorikanshiPlugin *mmk, const gchar *name,
             const gchar *val)
{
    gchar *path = prop_path(mmk, name);
    xfconf_channel_set_string(mmk->channel, path, val);
    g_free(path);
}

static void
write_string_list(MimamorikanshiPlugin *mmk, const gchar *name,
                  const gchar * const *list)
{
    gchar *path = prop_path(mmk, name);
    xfconf_channel_set_string_list(mmk->channel, path, list);
    g_free(path);
}

/* ── Widget signal callbacks ─────────────────────────────────────── */

typedef struct {
    MimamorikanshiPlugin *mmk;
    gchar *prop_name;
} PropCtx;

static void
prop_ctx_free(gpointer data, GClosure *closure G_GNUC_UNUSED)
{
    PropCtx *ctx = data;
    g_free(ctx->prop_name);
    g_free(ctx);
}

static PropCtx *
prop_ctx_new(MimamorikanshiPlugin *mmk, const gchar *prop_name)
{
    PropCtx *ctx = g_new0(PropCtx, 1);
    ctx->mmk       = mmk;
    ctx->prop_name = g_strdup(prop_name);
    return ctx;
}

static void
on_spin_changed(GtkSpinButton *spin, gpointer data)
{
    PropCtx *ctx = data;
    gint val = gtk_spin_button_get_value_as_int(spin);
    write_int(ctx->mmk, ctx->prop_name, val);
}

static void
on_color_set(GtkColorButton *btn, gpointer data)
{
    PropCtx *ctx = data;
    GdkRGBA rgba;
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(btn), &rgba);
    write_color(ctx->mmk, ctx->prop_name, &rgba);
}

static void
on_font_set(GtkFontButton *btn, gpointer data)
{
    PropCtx *ctx = data;
    const gchar *font = gtk_font_chooser_get_font(GTK_FONT_CHOOSER(btn));
    /* Extract family name (everything before the size) */
    write_string(ctx->mmk, ctx->prop_name, font);
}

static void
on_entry_activate(GtkEntry *entry, gpointer data)
{
    PropCtx *ctx = data;
    const gchar *text = gtk_entry_get_text(entry);
    /* Split comma-separated list */
    gchar **parts = g_strsplit(text, ",", -1);
    /* Trim whitespace */
    for (gint i = 0; parts[i]; i++)
        g_strstrip(parts[i]);
    write_string_list(ctx->mmk, ctx->prop_name, (const gchar * const *)parts);
    g_strfreev(parts);
}

static gboolean
on_entry_focus_out(GtkWidget *widget, GdkEventFocus *event G_GNUC_UNUSED,
                   gpointer data)
{
    on_entry_activate(GTK_ENTRY(widget), data);
    return FALSE;
}

/* ── Widget construction helpers ─────────────────────────────────── */

static GtkWidget *
add_section_label(GtkGrid *grid, gint *row, const gchar *text)
{
    gchar *markup = g_strdup_printf("<b>%s</b>", text);
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    g_free(markup);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_margin_top(label, 12);
    gtk_grid_attach(grid, label, 0, *row, 2, 1);
    (*row)++;
    return label;
}

static GtkWidget *
add_spin(GtkGrid *grid, gint *row, const gchar *label_text,
         MimamorikanshiPlugin *mmk, const gchar *prop,
         gint value, gint min, gint max, gint step)
{
    GtkWidget *label = gtk_label_new(label_text);
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(grid, label, 0, *row, 1, 1);

    GtkWidget *spin = gtk_spin_button_new_with_range(min, max, step);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), value);
    gtk_widget_set_hexpand(spin, TRUE);
    gtk_grid_attach(grid, spin, 1, *row, 1, 1);

    PropCtx *ctx = prop_ctx_new(mmk, prop);
    g_signal_connect_data(spin, "value-changed",
                          G_CALLBACK(on_spin_changed), ctx,
                          prop_ctx_free, 0);
    (*row)++;
    return spin;
}

static GtkWidget *
add_color(GtkGrid *grid, gint *row, const gchar *label_text,
          MimamorikanshiPlugin *mmk, const gchar *prop,
          const GdkRGBA *value)
{
    GtkWidget *label = gtk_label_new(label_text);
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(grid, label, 0, *row, 1, 1);

    GtkWidget *btn = gtk_color_button_new_with_rgba(value);
    gtk_widget_set_hexpand(btn, TRUE);
    gtk_grid_attach(grid, btn, 1, *row, 1, 1);

    PropCtx *ctx = prop_ctx_new(mmk, prop);
    g_signal_connect_data(btn, "color-set",
                          G_CALLBACK(on_color_set), ctx,
                          prop_ctx_free, 0);
    (*row)++;
    return btn;
}

static GtkWidget *
add_entry(GtkGrid *grid, gint *row, const gchar *label_text,
          MimamorikanshiPlugin *mmk, const gchar *prop,
          gchar **values, gint n_values)
{
    GtkWidget *label = gtk_label_new(label_text);
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(grid, label, 0, *row, 1, 1);

    gchar *joined = NULL;
    if (values && n_values > 0)
        joined = g_strjoinv(", ", values);

    GtkWidget *entry = gtk_entry_new();
    if (joined)
        gtk_entry_set_text(GTK_ENTRY(entry), joined);
    g_free(joined);
    gtk_widget_set_hexpand(entry, TRUE);
    gtk_grid_attach(grid, entry, 1, *row, 1, 1);

    PropCtx *ctx = prop_ctx_new(mmk, prop);
    g_signal_connect_data(entry, "activate",
                          G_CALLBACK(on_entry_activate), ctx,
                          prop_ctx_free, 0);
    /* Also update on focus-out – needs its own ctx and correct signature */
    PropCtx *ctx2 = prop_ctx_new(mmk, prop);
    g_signal_connect_data(entry, "focus-out-event",
                          G_CALLBACK(on_entry_focus_out), ctx2,
                          prop_ctx_free, 0);
    (*row)++;
    return entry;
}

/* ── Public entry point ──────────────────────────────────────────── */

void
mimamorikanshi_dialog_show(MimamorikanshiPlugin *mmk)
{
    const MimamorikanshiConfig *cfg = &mmk->config;

    GtkWidget *dialog = xfce_titled_dialog_new_with_mixed_buttons(
        "MimamoriKanshi Settings",
        GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(mmk->panel_plugin))),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        "window-close", "_Close", GTK_RESPONSE_OK,
        NULL);

    gtk_window_set_default_size(GTK_WINDOW(dialog), 420, 520);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    /* Scrolled window wrapping a grid */
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(content), scroll, TRUE, TRUE, 0);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_widget_set_margin_start(grid, 12);
    gtk_widget_set_margin_end(grid, 12);
    gtk_widget_set_margin_top(grid, 6);
    gtk_widget_set_margin_bottom(grid, 6);
    gtk_container_add(GTK_CONTAINER(scroll), grid);

    gint row = 0;

    /* ── Dimensions ── */
    add_section_label(GTK_GRID(grid), &row, "Dimensions");
    add_spin(GTK_GRID(grid), &row, "Widget width (px):",
             mmk, "width-px", cfg->width_px, 10, 500, 1);
    add_spin(GTK_GRID(grid), &row, "Row height (px):",
             mmk, "height-px", cfg->height_px, 4, 200, 1);
    add_spin(GTK_GRID(grid), &row, "Latest-value width (px):",
             mmk, "latest-value-px", cfg->latest_value_px, 0, 100, 1);
    add_spin(GTK_GRID(grid), &row, "Border width (px):",
             mmk, "border-px", cfg->border_px, 0, 20, 1);
    add_spin(GTK_GRID(grid), &row, "Separator width (px):",
             mmk, "separator-px", cfg->separator_px, 0, 20, 1);

    /* ── Colors ── */
    add_section_label(GTK_GRID(grid), &row, "Colors");
    add_color(GTK_GRID(grid), &row, "Background:",
              mmk, "background-color", &cfg->background_color);
    add_color(GTK_GRID(grid), &row, "Border:",
              mmk, "border-color", &cfg->border_color);
    add_color(GTK_GRID(grid), &row, "Separator:",
              mmk, "separator-color", &cfg->separator_color);

    /* ── Text ── */
    add_section_label(GTK_GRID(grid), &row, "Text");
    add_color(GTK_GRID(grid), &row, "Text color:",
              mmk, "text-color", &cfg->text_color);
    add_spin(GTK_GRID(grid), &row, "Text left padding (px):",
             mmk, "text-left-padding-px", cfg->text_left_padding_px, 0, 50, 1);
    add_spin(GTK_GRID(grid), &row, "Font size (pt):",
             mmk, "text-font-size", cfg->text_font_size, 4, 72, 1);

    /* Font button */
    {
        GtkWidget *label = gtk_label_new("Font:");
        gtk_widget_set_halign(label, GTK_ALIGN_END);
        gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);

        gchar *font_desc = g_strdup_printf("%s %d",
            cfg->text_font_family ? cfg->text_font_family : "monospace",
            cfg->text_font_size);
        GtkWidget *btn = gtk_font_button_new_with_font(font_desc);
        g_free(font_desc);
        gtk_widget_set_hexpand(btn, TRUE);
        gtk_grid_attach(GTK_GRID(grid), btn, 1, row, 1, 1);

        PropCtx *ctx = prop_ctx_new(mmk, "text-font-family");
        g_signal_connect_data(btn, "font-set",
                              G_CALLBACK(on_font_set), ctx,
                              prop_ctx_free, 0);
        row++;
    }

    /* ── CPU ── */
    add_section_label(GTK_GRID(grid), &row, "CPU");
    add_color(GTK_GRID(grid), &row, "CPU color:",
              mmk, "cpu-color", &cfg->cpu_color);

    /* ── Memory ── */
    add_section_label(GTK_GRID(grid), &row, "Memory");
    add_color(GTK_GRID(grid), &row, "Memory color:",
              mmk, "memory-color", &cfg->memory_color);

    /* ── Disk ── */
    add_section_label(GTK_GRID(grid), &row, "Disk");
    add_entry(GTK_GRID(grid), &row, "Disks (comma-sep):",
              mmk, "disks", cfg->disks, cfg->n_disks);
    add_color(GTK_GRID(grid), &row, "Disk read color:",
              mmk, "disk-read-color", &cfg->disk_read_color);
    add_spin(GTK_GRID(grid), &row, "Disk read max (MiB/s):",
             mmk, "disk-read-max-mib-s", cfg->disk_read_max_mib_s, 1, 100000, 10);
    add_color(GTK_GRID(grid), &row, "Disk write color:",
              mmk, "disk-write-color", &cfg->disk_write_color);
    add_spin(GTK_GRID(grid), &row, "Disk write max (MiB/s):",
             mmk, "disk-write-max-mib-s", cfg->disk_write_max_mib_s, 1, 100000, 10);

    /* ── Network ── */
    add_section_label(GTK_GRID(grid), &row, "Network");
    add_entry(GTK_GRID(grid), &row, "Interfaces (comma-sep):",
              mmk, "networks", cfg->networks, cfg->n_networks);
    add_color(GTK_GRID(grid), &row, "Download color:",
              mmk, "network-download-color", &cfg->network_download_color);
    add_spin(GTK_GRID(grid), &row, "Download max (MiB/s):",
             mmk, "network-download-max-mib-s", cfg->network_download_max_mib_s,
             1, 100000, 10);
    add_color(GTK_GRID(grid), &row, "Upload color:",
              mmk, "network-upload-color", &cfg->network_upload_color);
    add_spin(GTK_GRID(grid), &row, "Upload max (MiB/s):",
             mmk, "network-upload-max-mib-s", cfg->network_upload_max_mib_s,
             1, 100000, 10);

    /* ── Timing ── */
    add_section_label(GTK_GRID(grid), &row, "Timing");
    add_spin(GTK_GRID(grid), &row, "Update interval (ms):",
             mmk, "update-interval-ms", cfg->update_interval_ms, 50, 60000, 50);
    add_spin(GTK_GRID(grid), &row, "Suspend after (ms, 0=off):",
             mmk, "suspend-after-ms", cfg->suspend_after_ms, 0, 3600000, 1000);
    add_spin(GTK_GRID(grid), &row, "Suspend poll (ms):",
             mmk, "suspend-poll-ms", cfg->suspend_poll_ms, 500, 60000, 500);

    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}
