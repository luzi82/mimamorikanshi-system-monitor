# MimamoriKanshi System Monitor

A lightweight system monitor widget for the Xfce panel. Displays CPU, memory, disk, and network usage as a compact indicator image via [xfce4-genmon-plugin](https://docs.xfce.org/panel-plugins/xfce4-genmon-plugin/start).

## Indicator

Six rows rendered as a PPM P6 image:

| Row | Metric |
|-----|--------|
| 1 | CPU % |
| 2 | Memory % |
| 3 | Disk read MiB/s |
| 4 | Disk write MiB/s |
| 5 | Network download MiB/s |
| 6 | Network upload MiB/s |

Each row shows a scrolling history graph with a numeric overlay of the current value.

## Requirements

- Linux with `/proc` filesystem
- Python 3
- [Pillow](https://pypi.org/project/Pillow/) — `pip install Pillow` or `apt install python3-pil`
- [PyYAML](https://pypi.org/project/PyYAML/) — `pip install PyYAML` or `apt install python3-yaml`
- [xfce4-genmon-plugin](https://docs.xfce.org/panel-plugins/xfce4-genmon-plugin/start)

## Setup

1. Clone this repository.

2. Edit `config.yaml` to match your system:
   - **disks** — disk names from `/proc/diskstats` (e.g. `sda`, `nvme0n1`)
   - **networks** — interface names from `/proc/net/dev` (e.g. `eth0`, `wlan0`)
   - **tick_ms** — should match the xfce4-genmon-plugin period (in ms)
   - Adjust colors, font, and max throughput values as desired.

3. Add a **Generic Monitor** (genmon) panel item in Xfce and set its command to:

   ```
   /path/to/genmon.sh
   ```

4. Set the genmon period to match `tick_ms` in `config.yaml` (e.g. 500 ms).

## Architecture

```
xfce4-genmon-plugin
        │
        ▼
    genmon.sh  ──launches──▶  daemon.py
        │                        │
        │ reads                  │ writes
        ▼                        ▼
   /dev/shm/mimamorikanshi/
   ├── lock            # flock coordination
   ├── DAEMON.run      # daemon PID + start_time
   ├── DAEMON.out      # timestamp + last image ID
   ├── GENMON.run      # genmon PID + start_time
   ├── GENMON.out      # timestamp + last echoed image ID
   ├── img.0.ppm       # triple-buffered
   ├── img.1.ppm       #   indicator
   └── img.2.ppm       #   images
```

- **genmon.sh** is invoked periodically by the panel plugin. It starts the daemon if needed and echoes the latest image path.
- **daemon.py** runs in the background, reads `/proc/*` for system metrics, renders the indicator image, and writes it to the shared memory directory.
- A non-blocking `flock` on the lock file protects all shared file reads and writes.
- Three image buffers prevent the panel from reading a partially written file.

## Files

| File | Description |
|------|-------------|
| `genmon.sh` | GENMON — Bash script called by xfce4-genmon-plugin |
| `daemon.py` | DAEMON — Python background monitor and image renderer |
| `config.yaml` | User configuration |
| `doc/SPEC.md` | Full specification |

## License

See repository for license details.
