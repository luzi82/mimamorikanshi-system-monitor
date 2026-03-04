/*
 * MimamoriKanshi System Monitor - /proc/stat Parser
 *
 * Reads /proc/stat and computes CPU usage deltas.
 */

#include <stdio.h>
#include <string.h>
#include "proc_stat_parser.h"

/* ── Parse ───────────────────────────────────────────────────────── */

void
proc_stat_parse(const char *path, ProcStatData *out)
{
    memset(out, 0, sizeof(*out));
    out->valid = FALSE;

    FILE *fp = fopen(path, "r");
    if (!fp)
        return;

    char line[512];
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return;
    }
    fclose(fp);

    /* cpu  user nice system idle iowait irq softirq steal guest guest_nice */
    int n = sscanf(line, "cpu %lu %lu %lu %lu %lu %lu %lu %lu",
                   &out->user, &out->nice, &out->system, &out->idle,
                   &out->iowait, &out->irq, &out->softirq, &out->steal);

    out->valid = (n == 8);
}


