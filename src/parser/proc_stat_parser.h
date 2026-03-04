/*
 * MimamoriKanshi System Monitor - /proc/stat Parser
 *
 * Reads and parses /proc/stat to compute CPU usage.
 */

#ifndef MIMAMORIKANSHI_PROC_STAT_PARSER_H
#define MIMAMORIKANSHI_PROC_STAT_PARSER_H

#include <glib.h>

/*
 * Raw CPU counters from the first "cpu" line of /proc/stat.
 * valid is FALSE when the file could not be read or parsed.
 */
typedef struct {
    guint64  user;
    guint64  nice;
    guint64  system;
    guint64  idle;
    guint64  iowait;
    guint64  irq;
    guint64  softirq;
    guint64  steal;
    gboolean valid;
} ProcStatData;

/*
 * Open /proc/stat, read the aggregate "cpu" line and populate @out.
 * Sets out->valid = FALSE on any read or parse error.
 */
void proc_stat_parse(const char *path, ProcStatData *out);

#endif /* MIMAMORIKANSHI_PROC_STAT_PARSER_H */
