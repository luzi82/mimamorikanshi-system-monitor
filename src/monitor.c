/*
 * MimamoriKanshi System Monitor - Monitor Implementation
 *
 * Reads /proc/stat, /proc/meminfo, /proc/diskstats, /proc/net/dev
 * and computes deltas using monotonic time.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "plugin.h"

#define DEFAULT_SECTOR_SIZE 512
#define BYTES_PER_MIB       (1024.0 * 1024.0)

/* ── Helpers ─────────────────────────────────────────────────────── */

static inline gdouble
clamp01(gdouble v)
{
    return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v);
}

/* ── Init / Free ─────────────────────────────────────────────────── */

void
mimamorikanshi_monitor_init(MonitorState *state)
{
    memset(state, 0, sizeof(*state));
    state->sector_sizes = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                 g_free, NULL);
    state->baseline_valid = FALSE;
}

void
mimamorikanshi_monitor_free(MonitorState *state)
{
    if (state->sector_sizes) {
        g_hash_table_destroy(state->sector_sizes);
        state->sector_sizes = NULL;
    }
}

void
mimamorikanshi_monitor_reset_baseline(MonitorState *state)
{
    state->baseline_valid = FALSE;
    state->prev_time_us   = 0;
    g_debug("mimamorikanshi: monitor baseline reset");
}

/* ── Sector sizes ────────────────────────────────────────────────── */

void
mimamorikanshi_monitor_refresh_sector_sizes(MonitorState *state,
                                             gchar       **disks,
                                             gint          n_disks)
{
    g_hash_table_remove_all(state->sector_sizes);

    for (gint i = 0; i < n_disks; i++) {
        gchar *path = g_strdup_printf("/sys/block/%s/queue/logical_block_size",
                                       disks[i]);
        FILE *fp = fopen(path, "r");
        guint sector_size = DEFAULT_SECTOR_SIZE;

        if (fp) {
            if (fscanf(fp, "%u", &sector_size) != 1) {
                g_warning("mimamorikanshi: could not parse %s, using default %d",
                          path, DEFAULT_SECTOR_SIZE);
                sector_size = DEFAULT_SECTOR_SIZE;
            }
            fclose(fp);
        } else {
            g_warning("mimamorikanshi: could not open %s, using default %d",
                      path, DEFAULT_SECTOR_SIZE);
        }
        g_free(path);

        g_hash_table_insert(state->sector_sizes,
                            g_strdup(disks[i]),
                            GUINT_TO_POINTER(sector_size));
    }
}

/* ── CPU ─────────────────────────────────────────────────────────── */

static void
read_cpu(MonitorState *state, gdouble *out_pct)
{
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) {
        *out_pct = 0.0;
        return;
    }

    char line[512];
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        *out_pct = 0.0;
        return;
    }
    fclose(fp);

    /* cpu  user nice system idle iowait irq softirq steal guest guest_nice */
    guint64 user, nice, system, idle, iowait, irq, softirq, steal;
    user = nice = system = idle = iowait = irq = softirq = steal = 0;

    sscanf(line, "cpu %lu %lu %lu %lu %lu %lu %lu %lu",
           &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);

    guint64 total = user + nice + system + idle + iowait + irq + softirq + steal;
    guint64 idle_all = idle + iowait;

    if (state->baseline_valid) {
        guint64 dtotal = total - state->prev_cpu_total;
        guint64 didle  = idle_all - state->prev_cpu_idle;
        if (dtotal > 0)
            *out_pct = 100.0 * (1.0 - (gdouble)didle / (gdouble)dtotal);
        else
            *out_pct = 0.0;
    } else {
        *out_pct = 0.0;
    }

    state->prev_cpu_total = total;
    state->prev_cpu_idle  = idle_all;
}

/* ── Memory ──────────────────────────────────────────────────────── */

static void
read_memory(gdouble *out_pct)
{
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        *out_pct = 0.0;
        return;
    }

    guint64 mem_total = 0, mem_available = 0;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "MemTotal: %lu kB", &mem_total) == 1)
            continue;
        if (sscanf(line, "MemAvailable: %lu kB", &mem_available) == 1)
            continue;
    }
    fclose(fp);

    if (mem_total > 0)
        *out_pct = 100.0 * (gdouble)(mem_total - mem_available) / (gdouble)mem_total;
    else
        *out_pct = 0.0;
}

/* ── Disk ────────────────────────────────────────────────────────── */

static void
read_disk(MonitorState *state, gchar **disks, gint n_disks,
          gdouble dt_sec,
          gdouble *out_read_mib_s, gdouble *out_write_mib_s)
{
    guint64 sectors_read  = 0;
    guint64 sectors_written = 0;

    FILE *fp = fopen("/proc/diskstats", "r");
    if (!fp) {
        *out_read_mib_s  = 0.0;
        *out_write_mib_s = 0.0;
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        unsigned int major, minor;
        char devname[128];
        guint64 rd, wr;
        /* Fields: major minor name reads_completed reads_merged sectors_read
                   ms_reading writes_completed writes_merged sectors_written ... */
        guint64 dummy;
        if (sscanf(line, " %u %u %127s %lu %lu %lu %lu %lu %lu %lu",
                   &major, &minor, devname,
                   &dummy, &dummy, &rd, &dummy,
                   &dummy, &dummy, &wr) < 10)
            continue;

        for (gint i = 0; i < n_disks; i++) {
            if (g_strcmp0(devname, disks[i]) == 0) {
                gpointer p = g_hash_table_lookup(state->sector_sizes, devname);
                guint sector_size = p ? GPOINTER_TO_UINT(p) : DEFAULT_SECTOR_SIZE;
                sectors_read    += rd * sector_size;
                sectors_written += wr * sector_size;
                break;
            }
        }
    }
    fclose(fp);

    /* sectors_read/written now hold total bytes (sector_count * sector_size) */
    if (state->baseline_valid && dt_sec > 0.0) {
        gdouble dr = (gdouble)(sectors_read  - state->prev_disk_sectors_read);
        gdouble dw = (gdouble)(sectors_written - state->prev_disk_sectors_written);
        *out_read_mib_s  = (dr / BYTES_PER_MIB) / dt_sec;
        *out_write_mib_s = (dw / BYTES_PER_MIB) / dt_sec;
    } else {
        *out_read_mib_s  = 0.0;
        *out_write_mib_s = 0.0;
    }

    state->prev_disk_sectors_read    = sectors_read;
    state->prev_disk_sectors_written = sectors_written;
}

/* ── Network ─────────────────────────────────────────────────────── */

static void
read_network(MonitorState *state, gchar **networks, gint n_networks,
             gdouble dt_sec,
             gdouble *out_dl_mbit_s, gdouble *out_ul_mbit_s)
{
    guint64 bytes_rx = 0;
    guint64 bytes_tx = 0;

    FILE *fp = fopen("/proc/net/dev", "r");
    if (!fp) {
        *out_dl_mbit_s = 0.0;
        *out_ul_mbit_s = 0.0;
        return;
    }

    char line[512];
    /* Skip header lines */
    if (fgets(line, sizeof(line), fp) == NULL) { *out_dl_mbit_s = 0.0; *out_ul_mbit_s = 0.0; fclose(fp); return; }
    if (fgets(line, sizeof(line), fp) == NULL) { *out_dl_mbit_s = 0.0; *out_ul_mbit_s = 0.0; fclose(fp); return; }

    while (fgets(line, sizeof(line), fp)) {
        /* Format:  iface: rx_bytes rx_packets ... tx_bytes tx_packets ... */
        char *colon = strchr(line, ':');
        if (!colon)
            continue;

        /* Extract interface name (trim leading whitespace) */
        char *p = line;
        while (*p == ' ')
            p++;
        gchar *ifname = g_strndup(p, (gsize)(colon - p));
        g_strstrip(ifname);

        gboolean match = FALSE;
        for (gint i = 0; i < n_networks; i++) {
            if (g_strcmp0(ifname, networks[i]) == 0) {
                match = TRUE;
                break;
            }
        }

        if (match) {
            guint64 rx, tx;
            guint64 d1, d2, d3, d4, d5, d6;
            /* 8 receive fields then 8 transmit fields */
            if (sscanf(colon + 1,
                       " %lu %lu %lu %lu %lu %lu %lu %lu"
                       " %lu %lu %lu %lu %lu %lu %lu %lu",
                       &rx, &d1, &d2, &d3, &d4, &d5, &d6, &d6,
                       &tx, &d1, &d2, &d3, &d4, &d5, &d6, &d6) >= 10) {
                bytes_rx += rx;
                bytes_tx += tx;
            }
        }
        g_free(ifname);
    }
    fclose(fp);

    if (state->baseline_valid && dt_sec > 0.0) {
        gdouble dr = (gdouble)(bytes_rx - state->prev_net_bytes_rx);
        gdouble dt = (gdouble)(bytes_tx - state->prev_net_bytes_tx);
        *out_dl_mbit_s = dr * 8.0 / 1000000.0 / dt_sec;
        *out_ul_mbit_s = dt * 8.0 / 1000000.0 / dt_sec;
    } else {
        *out_dl_mbit_s = 0.0;
        *out_ul_mbit_s = 0.0;
    }

    state->prev_net_bytes_rx = bytes_rx;
    state->prev_net_bytes_tx = bytes_tx;
}

/* ── Main update ─────────────────────────────────────────────────── */

void
mimamorikanshi_monitor_update(MimamorikanshiPlugin *mmk)
{
    MonitorState *st          = &mmk->monitor;
    MimamorikanshiConfig *cfg = &mmk->config;

    gint64 now_us = g_get_monotonic_time();
    gdouble dt_sec = 0.0;

    if (st->prev_time_us > 0)
        dt_sec = (gdouble)(now_us - st->prev_time_us) / 1000000.0;

    /* ── Collect raw samples ── */
    gdouble cpu_pct, mem_pct;
    gdouble disk_read_mib, disk_write_mib;
    gdouble net_dl_mbit, net_ul_mbit;

    read_cpu(st, &cpu_pct);
    read_memory(&mem_pct);
    read_disk(st, cfg->disks, cfg->n_disks, dt_sec,
              &disk_read_mib, &disk_write_mib);
    read_network(st, cfg->networks, cfg->n_networks, dt_sec,
                 &net_dl_mbit, &net_ul_mbit);

    st->prev_time_us = now_us;

    if (!st->baseline_valid) {
        /* First sample after init/reset – record baselines, no output */
        st->baseline_valid = TRUE;
        g_debug("mimamorikanshi: baseline recorded, discarding first sample");
        return;
    }

    /* ── Store raw values for text display ── */
    st->raw_values[ROW_CPU]          = cpu_pct;
    st->raw_values[ROW_MEMORY]       = mem_pct;
    st->raw_values[ROW_DISK_READ]    = disk_read_mib;
    st->raw_values[ROW_DISK_WRITE]   = disk_write_mib;
    st->raw_values[ROW_NET_DOWNLOAD] = net_dl_mbit;
    st->raw_values[ROW_NET_UPLOAD]   = net_ul_mbit;

    /* ── Normalize to 0.0–1.0 for graph bars ── */
    st->values[ROW_CPU]    = clamp01(cpu_pct / 100.0);
    st->values[ROW_MEMORY] = clamp01(mem_pct / 100.0);

    gdouble drm = cfg->disk_read_max_mib_s  > 0 ? cfg->disk_read_max_mib_s  : 100.0;
    gdouble dwm = cfg->disk_write_max_mib_s > 0 ? cfg->disk_write_max_mib_s : 100.0;
    gdouble ndm = cfg->network_download_max_mbit_s > 0 ? cfg->network_download_max_mbit_s : 100.0;
    gdouble num = cfg->network_upload_max_mbit_s   > 0 ? cfg->network_upload_max_mbit_s   : 100.0;

    st->values[ROW_DISK_READ]    = clamp01(disk_read_mib  / drm);
    st->values[ROW_DISK_WRITE]   = clamp01(disk_write_mib / dwm);
    st->values[ROW_NET_DOWNLOAD] = clamp01(net_dl_mbit    / ndm);
    st->values[ROW_NET_UPLOAD]   = clamp01(net_ul_mbit    / num);
}
