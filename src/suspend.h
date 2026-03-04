/*
 * MimamoriKanshi System Monitor - Suspend/Resume Header
 *
 * Detects user inactivity (panel hidden, screen locked) and suspends
 * expensive polling to conserve power.
 */

#ifndef MIMAMORIKANSHI_SUSPEND_H
#define MIMAMORIKANSHI_SUSPEND_H

#include <glib.h>
#include <gio/gio.h>

typedef struct _SuspendState SuspendState;

struct _SuspendState {
    /* Whether the plugin is currently suspended */
    gboolean suspended;

    /* Monotonic time (µs) when inactivity was first detected.
       0 means the plugin is currently considered active. */
    gint64 inactive_since_us;

    /* Lightweight poll timer used while suspended */
    guint poll_timer_id;

    /* D-Bus subscription IDs */
    guint screensaver_sub_id;
    GDBusConnection *session_bus;

    /* Externally-observed state flags */
    gboolean widget_visible;   /* TRUE if the drawing area is mapped */
    gboolean screen_locked;    /* TRUE if screen-saver signals "Active" */
};

/* Forward declaration */
typedef struct _MimamorikanshiPlugin MimamorikanshiPlugin;

/* Initialize suspend state and connect to D-Bus signals */
void mimamorikanshi_suspend_init(MimamorikanshiPlugin *mmk);

/* Clean up D-Bus subscriptions and poll timer */
void mimamorikanshi_suspend_free(MimamorikanshiPlugin *mmk);

/* Called in the regular timer tick to evaluate whether to enter suspend.
   Returns TRUE if the plugin should skip this tick (already suspended). */
gboolean mimamorikanshi_suspend_tick(MimamorikanshiPlugin *mmk);

/* GTK "map" signal handler for the drawing area */
void mimamorikanshi_suspend_on_map(GtkWidget *widget, gpointer data);

/* GTK "unmap" signal handler for the drawing area */
void mimamorikanshi_suspend_on_unmap(GtkWidget *widget, gpointer data);

#endif /* MIMAMORIKANSHI_SUSPEND_H */
