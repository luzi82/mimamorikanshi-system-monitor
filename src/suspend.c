/*
 * MimamoriKanshi System Monitor - Suspend/Resume Implementation
 *
 * Monitors widget visibility and D-Bus ScreenSaver signals to decide
 * when to suspend and resume polling.
 *
 * Suspend = stop the regular GLib timer, start a lightweight poll.
 * Resume  = stop the poll timer, reset baselines, restart the regular timer.
 */

#include "plugin.h"
#include "suspend.h"

/* ── Forward declarations from plugin.c ──────────────────────────── */
extern void mimamorikanshi_start_timer(MimamorikanshiPlugin *mmk);
extern void mimamorikanshi_stop_timer(MimamorikanshiPlugin *mmk);

/* ── Helpers ─────────────────────────────────────────────────────── */

static gboolean
is_inactive(const MimamorikanshiPlugin *mmk)
{
    const SuspendState *s = &mmk->suspend;
    return !s->widget_visible || s->screen_locked;
}

/* ── Resume logic ────────────────────────────────────────────────── */

static void
do_resume(MimamorikanshiPlugin *mmk)
{
    SuspendState *s = &mmk->suspend;

    if (!s->suspended)
        return;

    g_debug("mimamorikanshi: resuming from suspend");

    s->suspended       = FALSE;
    s->inactive_since_us = 0;

    /* Stop poll timer */
    if (s->poll_timer_id > 0) {
        g_source_remove(s->poll_timer_id);
        s->poll_timer_id = 0;
    }

    /* Reset monitor baselines – first sample after resume is discarded */
    mimamorikanshi_monitor_reset_baseline(&mmk->monitor);

    /* Restart regular timer */
    mimamorikanshi_start_timer(mmk);
}

/* ── Suspend logic ───────────────────────────────────────────────── */

static gboolean poll_timer_cb(gpointer data);

static void
do_suspend(MimamorikanshiPlugin *mmk)
{
    SuspendState *s = &mmk->suspend;

    if (s->suspended)
        return;

    g_debug("mimamorikanshi: entering suspend");

    s->suspended = TRUE;

    /* Stop regular timer */
    mimamorikanshi_stop_timer(mmk);

    /* Start lightweight poll timer */
    if (mmk->config.suspend_poll_ms > 0) {
        s->poll_timer_id = g_timeout_add(
            (guint)mmk->config.suspend_poll_ms,
            poll_timer_cb, mmk);
    }
}

/* ── Poll timer callback (runs while suspended) ─────────────────── */

static gboolean
poll_timer_cb(gpointer data)
{
    MimamorikanshiPlugin *mmk = data;

    if (!is_inactive(mmk)) {
        do_resume(mmk);
        return G_SOURCE_REMOVE;
    }

    return G_SOURCE_CONTINUE;
}

/* ── D-Bus ScreenSaver signal handler ────────────────────────────── */

static void
on_screensaver_active_changed(GDBusConnection *conn    G_GNUC_UNUSED,
                               const gchar     *sender G_GNUC_UNUSED,
                               const gchar     *path   G_GNUC_UNUSED,
                               const gchar     *iface  G_GNUC_UNUSED,
                               const gchar     *signal G_GNUC_UNUSED,
                               GVariant        *params,
                               gpointer         user_data)
{
    MimamorikanshiPlugin *mmk = user_data;

    gboolean active = FALSE;
    g_variant_get(params, "(b)", &active);

    mmk->suspend.screen_locked = active;
    g_debug("mimamorikanshi: ScreenSaver ActiveChanged → %s",
            active ? "locked" : "unlocked");

    /* Immediate resume check */
    if (!active && mmk->suspend.suspended)
        do_resume(mmk);
}

/* ── GTK map/unmap handlers ──────────────────────────────────────── */

void
mimamorikanshi_suspend_on_map(GtkWidget *widget G_GNUC_UNUSED,
                               gpointer   data)
{
    MimamorikanshiPlugin *mmk = data;
    mmk->suspend.widget_visible = TRUE;
    g_debug("mimamorikanshi: widget mapped (visible)");

    if (mmk->suspend.suspended && !is_inactive(mmk))
        do_resume(mmk);
}

void
mimamorikanshi_suspend_on_unmap(GtkWidget *widget G_GNUC_UNUSED,
                                 gpointer   data)
{
    MimamorikanshiPlugin *mmk = data;
    mmk->suspend.widget_visible = FALSE;
    g_debug("mimamorikanshi: widget unmapped (hidden)");
}

/* ── Init / Free ─────────────────────────────────────────────────── */

void
mimamorikanshi_suspend_init(MimamorikanshiPlugin *mmk)
{
    SuspendState *s = &mmk->suspend;

    s->suspended         = FALSE;
    s->inactive_since_us = 0;
    s->poll_timer_id     = 0;
    s->widget_visible    = TRUE; /* optimistic default */
    s->screen_locked     = FALSE;
    s->session_bus       = NULL;
    s->screensaver_sub_id = 0;

    /* Try to subscribe to org.freedesktop.ScreenSaver.ActiveChanged */
    GError *err = NULL;
    s->session_bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &err);
    if (s->session_bus) {
        s->screensaver_sub_id = g_dbus_connection_signal_subscribe(
            s->session_bus,
            NULL, /* any sender */
            "org.freedesktop.ScreenSaver",
            "ActiveChanged",
            "/org/freedesktop/ScreenSaver",
            NULL, /* no arg0 */
            G_DBUS_SIGNAL_FLAGS_NONE,
            on_screensaver_active_changed,
            mmk,
            NULL);
        g_debug("mimamorikanshi: subscribed to ScreenSaver ActiveChanged");
    } else {
        g_debug("mimamorikanshi: could not connect to session bus: %s",
                err ? err->message : "unknown");
        g_clear_error(&err);
    }
}

void
mimamorikanshi_suspend_free(MimamorikanshiPlugin *mmk)
{
    SuspendState *s = &mmk->suspend;

    if (s->poll_timer_id > 0) {
        g_source_remove(s->poll_timer_id);
        s->poll_timer_id = 0;
    }

    if (s->session_bus && s->screensaver_sub_id > 0) {
        g_dbus_connection_signal_unsubscribe(s->session_bus,
                                              s->screensaver_sub_id);
        s->screensaver_sub_id = 0;
    }

    /* Don't unref the shared bus connection (g_bus_get returns a shared ref) */
    s->session_bus = NULL;
}

/* ── Tick evaluation (called from the regular timer) ─────────────── */

gboolean
mimamorikanshi_suspend_tick(MimamorikanshiPlugin *mmk)
{
    SuspendState *s = &mmk->suspend;

    /* If already suspended, caller should not be calling us, but be safe */
    if (s->suspended)
        return TRUE;

    /* suspension is disabled */
    if (mmk->config.suspend_after_ms <= 0) {
        s->inactive_since_us = 0;
        return FALSE;
    }

    if (is_inactive(mmk)) {
        gint64 now = g_get_monotonic_time();
        if (s->inactive_since_us == 0)
            s->inactive_since_us = now;

        gint64 elapsed_ms = (now - s->inactive_since_us) / 1000;
        if (elapsed_ms >= mmk->config.suspend_after_ms) {
            do_suspend(mmk);
            return TRUE;
        }
    } else {
        s->inactive_since_us = 0;
    }

    return FALSE;
}
