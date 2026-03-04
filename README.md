# MimamoriKanshi System Monitor

A lightweight native Xfce panel plugin that monitors system metrics with scrolling history graphs.

## Monitored Metrics

- **CPU** utilization %
- **Memory** utilization %
- **Disk** read/write throughput (MiB/s)
- **Network** download/upload throughput (Mbit/s)

## Features

- 6-row scrolling history graph rendered with Cairo
- Per-row circular image buffer for efficient drawing
- Latest-value bar and numeric text overlay
- Configurable colors, dimensions, fonts, and device lists via Xfconf
- Settings dialog accessible from the panel plugin menu
- Automatic suspend/resume on screen lock or panel hide to save power
- First-sample spike suppression after init and resume

## Dependencies

- GTK+ 3.0
- libxfce4panel-2.0
- libxfce4ui-2.0
- libxfconf-0
- Cairo

## Building

```sh
meson setup builddir
ninja -C builddir
```

## Installing

```sh
sudo ninja -C builddir install
```

Then add **MimamoriKanshi System Monitor** from the Xfce panel's "Add New Items" dialog.

## Configuration

Right-click the plugin in the panel and select **Properties** to open the settings dialog. All changes take effect immediately.

See [doc/SPEC.md](doc/SPEC.md) for the full specification.

## License

GPL-2.0-or-later
