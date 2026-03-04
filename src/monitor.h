/*
 * MimamoriKanshi System Monitor - Monitor Header
 *
 * System metric collection from /proc filesystem.
 */

#ifndef MIMAMORIKANSHI_MONITOR_H
#define MIMAMORIKANSHI_MONITOR_H

#include <glib.h>
#include "common.h"
#include "parser/proc_stat_parser.h"

typedef struct _MonitorState MonitorState;

struct _MonitorState {
    /* CPU (from /proc/stat) */
    guint64 prev_cpu_total;
    guint64 prev_cpu_idle;

    /* Disk (from /proc/diskstats) */
    guint64 prev_disk_sectors_read;
    guint64 prev_disk_sectors_written;
    GHashTable *sector_sizes; /* gchar* device name → GUINT_TO_POINTER(sector_size) */

    /* Network (from /proc/net/dev) */
    guint64 prev_net_bytes_rx;
    guint64 prev_net_bytes_tx;

    /* Timing */
    gint64 prev_time_us; /* monotonic microseconds */

    /* Current values – normalized 0.0–1.0 for graph height */
    gdouble values[NUM_ROWS];

    /* Raw values for text overlay display */
    gdouble raw_values[NUM_ROWS];

    /* TRUE once we have a valid previous sample to delta against */
    gboolean baseline_valid;
};

/* Forward declaration */
typedef struct _MimamorikanshiPlugin MimamorikanshiPlugin;

/* Initialize monitor state (zero counters, create hash table) */
void mimamorikanshi_monitor_init(MonitorState *state);

/* Free monitor state resources */
void mimamorikanshi_monitor_free(MonitorState *state);

/* Collect one sample of all metrics.  On the first call after init or
   reset the function records baselines and leaves values[] at zero. */
void mimamorikanshi_monitor_update(MimamorikanshiPlugin *mmk);

/* Invalidate baselines so the next update is treated as the first sample
   (used on resume after suspend). */
void mimamorikanshi_monitor_reset_baseline(MonitorState *state);

/* Read /sys/block/<dev>/queue/logical_block_size for every device in the
   list and cache results.  Falls back to 512 if unreadable. */
void mimamorikanshi_monitor_refresh_sector_sizes(MonitorState *state,
                                                  gchar **disks,
                                                  gint    n_disks);

#endif /* MIMAMORIKANSHI_MONITOR_H */
