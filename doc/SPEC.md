The following is the updated spec of software project MimamoriKanshi-system-monitor

# SPEC - MimamoriKanshi System Monitor (C/GTK Version)

## Aim

* A native Xfce panel plugin to monitor the following system metrics:
  * CPU %
  * Memory %
  * Disk read/write throughput
  * Network upload/download throughput

## Design

* **Integrated Plugin**: A single C executable implemented as a native Xfce panel plugin using `libxfce4panel-2.0`.
* **UI Framework**: GTK3 for widget management and lifecycle.
* **Rendering**: Direct drawing using Cairo. No external image files or shared memory buffers are used.
* **Configuration**: Standard Xfce configuration storage via **Xfconf**.
* **Data Collection**: Periodic polling of `/proc` filesystem.

## Visual Design

* Designed primarily for use in a **vertical panel**.
* **6 rows** of monitoring data:
  1. CPU %
  2. Memory %
  3. Disk read MiB/s
  4. Disk write MiB/s
  5. Network download MiB/s
  6. Network upload MiB/s
* **For each row**:
  * **Scrolling History**: A graph where the x-axis represents time (1 pixel per update) and the y-axis represents the value. Implement this using a background per-row circular image buffer (Cairo image surface) to avoid full-widget redraws on every tick: allocate a cairo image surface sized to the graph area, maintain a write index (x position) that advances by 1 pixel per update, draw the new vertical column (latest-value bar) into the surface at the current index, then composite the surface into the widget using the appropriate offset so the visual output appears continuously scrolling. Create or recreate the image surfaces on startup and on resize; cache the surfaces and reuse them between ticks. If the image-surface path is unavailable (e.g., allocation failure), fall back to a safe full redraw implementation.
  * **Latest Value**: The rightmost section (`latest-value-px` width) displays the most recent value as a solid bar.
  * **Text Overlay**: Current numeric value displayed on the left, rounding to the nearest integer.
  * **Scaling**: For CPU/Memory, max is 100%. For Disk/Network, the max is configurable; values exceeding max are clipped to the top of the graph.

## Implementation Details

### Monitoring Logic
* **CPU**: Read from `/proc/stat`, calculate utilization based on the difference between successive reads.
* **Memory**: Read from `/proc/meminfo` (Total - Available).
* **Disk**: Read from `/proc/diskstats`. Sum the sectors read/written for all configured disks and convert to MiB/s. To accurately convert sector counts to bytes, the plugin MUST read the per-device sector size from `/sys/block/<dev>/queue/logical_block_size` at program start and whenever the configured `disks` list changes. Use the reported `logical_block_size` (bytes per sector) to convert sectors → bytes → MiB/s. Cache per-device sector sizes and refresh them on configuration changes. If a sysfs entry is unavailable or unreadable, fall back to a sensible default (512 bytes) and log a warning.
* **Network**: Read from `/proc/net/dev`. Sum the bytes received/transmitted for all configured interfaces and convert to MiB/s.

### Configuration (Xfconf)
Properties are stored in the `xfce4-panel` channel under the base path `/plugins/panel/mimamorikanshi-N/` (where `N` is the plugin instance ID). Standard Xfce practice is to bind these properties directly to the UI widgets for immediate updates.

| Property | Type | Description |
|----------|------|-------------|
| `width-px` | int | Total widget width |
| `height-px` | int | Height per data row |
| `latest-value-px` | int | Width of the latest value bar |
| `background-color` | string | Hex color (e.g., `#000000`) |
| `border-px` | int | Width of the widget border |
| `border-color` | string | Hex color |
| `separator-px` | int | Width of the separator between rows |
| `separator-color` | string | Hex color |
| `text-color` | string | Hex color |
| `text-left-padding-px` | int | Horizontal padding for text overlay |
| `text-font-family` | string | Font family name |
| `text-font-size` | int | Font size in points |
| `cpu-color` | string | Hex color for CPU graph |
| `memory-color` | string | Hex color for Memory graph |
| `disks` | array(string)| List of disk IDs (e.g., `sda`, `nvme0n1`) |
| `disk-read-color` | string | Hex color |
| `disk-read-max-mib-s` | int | Y-axis scale for disk read |
| `disk-write-color` | string | Hex color |
| `disk-write-max-mib-s` | int | Y-axis scale for disk write |
| `networks` | array(string)| List of interface names (e.g., `eth0`, `wlan0`) |
| `network-download-color` | string | Hex color |
| `network-download-max-mib-s` | int | Y-axis scale for download |
| `network-upload-color` | string | Hex color |
| `network-upload-max-mib-s` | int | Y-axis scale for upload |
| `update-interval-ms` | int | Polling interval (default: 500) |

### Configuration UI
A settings dialog is provided to modify the plugin configuration. It follows Xfce design guidelines:
* **Architecture**: A standard Xfce settings dialog implemented using `libxfce4ui-2.0`.
* **Layout**: A single scrollable list of settings.
* **Widgets**:
  * **Colors**: `GtkColorButton` for all color selections.
  * **Fonts**: `GtkFontButton` for text styling.
  * **Dimensions/Intervals**: `GtkSpinButton` for numeric values.
  * **Devices**: `GtkEntry` for comma-separated or list-based device and network interface selection.
* **Behavior**:
  * Changes are applied **immediately** to Xfconf upon widget interaction.
  * The dialog is launched from the standard Xfce panel plugin "Properties" menu item.

### Build Requirements
* **Build System**: Meson & Ninja
* **Dependencies**:
  * `gtk+-3.0`
  * `libxfce4panel-2.0`
  * `libxfce4ui-2.0`
  * `libxfconf-0`
  * `libcairo2`

## Remark
* The plugin should be lightweight, minimizing CPU wakeups by using GLib timers aligned with the desired `update-interval-ms`.
* Cairo surfaces should be managed efficiently to avoid unnecessary re-allocations on every tick.
* Configuration changes in Xfconf should be handled via "property-changed" signals to allow real-time updates without restarting the panel.

===

For the above spec:
* Comment
* Check typo
* Suggest improvement
* Ask question
