/*
 * MimamoriKanshi System Monitor - Common Definitions
 *
 * Shared constants and types used across all modules.
 */

#ifndef MIMAMORIKANSHI_COMMON_H
#define MIMAMORIKANSHI_COMMON_H

#define NUM_ROWS 6

typedef enum {
    ROW_CPU = 0,
    ROW_MEMORY,
    ROW_DISK_READ,
    ROW_DISK_WRITE,
    ROW_NET_DOWNLOAD,
    ROW_NET_UPLOAD,
} RowIndex;

#endif /* MIMAMORIKANSHI_COMMON_H */
