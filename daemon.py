#!/usr/bin/env python3
"""MimamoriKanshi System Monitor - DAEMON

Background monitor daemon that:
- Monitors CPU, memory, disk, and network usage from /proc
- Generates indicator PPM P6 images
- Coordinates with GENMON via shared files in /dev/shm/mimamorikanshi
"""

import collections
import fcntl
import os
import sys
import time

import yaml
from PIL import Image, ImageDraw, ImageFont

WORK_DIR = "/dev/shm/mimamorikanshi"
LOCK_FILE = os.path.join(WORK_DIR, "lock")
DAEMON_RUN = os.path.join(WORK_DIR, "DAEMON.run")
DAEMON_OUT = os.path.join(WORK_DIR, "DAEMON.out")
GENMON_OUT = os.path.join(WORK_DIR, "GENMON.out")

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
CONFIG_PATH = os.path.join(SCRIPT_DIR, "config.yaml")


# ---------------------------------------------------------------------------
# Process utilities
# ---------------------------------------------------------------------------

def get_process_start_time(pid):
    """Get process start time from /proc/[pid]/stat (field 22)."""
    try:
        with open(f"/proc/{pid}/stat", "r") as f:
            parts = f.read().split()
            return parts[21]  # 0-indexed → field 22
    except (FileNotFoundError, IndexError, PermissionError):
        return None


def is_process_running(pid, start_time):
    """Check if a process with the given PID and start_time is still alive."""
    return get_process_start_time(pid) == start_time


# ---------------------------------------------------------------------------
# Lock-protected file I/O
# ---------------------------------------------------------------------------

def read_locked(lock_fd, filepath):
    """Read file content with non-blocking flock protection.

    Returns file content as string, or None on lock failure / missing file.
    """
    try:
        fcntl.flock(lock_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except (IOError, OSError):
        return None
    try:
        if os.path.exists(filepath):
            with open(filepath, "r") as f:
                return f.read()
        return None
    finally:
        fcntl.flock(lock_fd, fcntl.LOCK_UN)


def write_locked(lock_fd, filepath, content):
    """Write string content atomically with non-blocking flock protection.

    Writes to a .tmp file first, then replaces the target under lock.
    """
    tmp_path = filepath + ".tmp"
    with open(tmp_path, "w") as f:
        f.write(content)
    try:
        fcntl.flock(lock_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except (IOError, OSError):
        # Best-effort: replace anyway (os.replace is atomic on same fs)
        os.replace(tmp_path, filepath)
        return
    try:
        os.replace(tmp_path, filepath)
    finally:
        fcntl.flock(lock_fd, fcntl.LOCK_UN)


def write_binary_locked(lock_fd, filepath, data):
    """Write binary content atomically with non-blocking flock protection."""
    tmp_path = filepath + ".tmp"
    with open(tmp_path, "wb") as f:
        f.write(data)
    try:
        fcntl.flock(lock_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except (IOError, OSError):
        os.replace(tmp_path, filepath)
        return
    try:
        os.replace(tmp_path, filepath)
    finally:
        fcntl.flock(lock_fd, fcntl.LOCK_UN)


# ---------------------------------------------------------------------------
# Key=value helpers
# ---------------------------------------------------------------------------

def parse_kv(content):
    """Parse 'key=value' text into a dict."""
    result = {}
    if content:
        for line in content.strip().split("\n"):
            if "=" in line:
                k, v = line.split("=", 1)
                result[k.strip()] = v.strip()
    return result


def format_kv(d):
    """Format a dict as 'key=value' lines."""
    return "\n".join(f"{k}={v}" for k, v in d.items()) + "\n"


# ---------------------------------------------------------------------------
# /proc readers
# ---------------------------------------------------------------------------

def read_cpu():
    """Read aggregate CPU counters from /proc/stat.

    Returns (total_ticks, idle_ticks).
    """
    with open("/proc/stat", "r") as f:
        line = f.readline()
    # cpu  user nice system idle iowait irq softirq steal [guest guest_nice]
    values = [int(x) for x in line.split()[1:]]
    total = sum(values)
    idle = values[3] + values[4]  # idle + iowait
    return total, idle


def read_memory():
    """Read memory usage from /proc/meminfo.

    Returns used percentage (0-100).
    """
    mem = {}
    with open("/proc/meminfo", "r") as f:
        for line in f:
            parts = line.split()
            if len(parts) >= 2:
                mem[parts[0].rstrip(":")] = int(parts[1])
    total = mem.get("MemTotal", 1)
    available = mem.get("MemAvailable", 0)
    return (total - available) / total * 100.0


def read_diskstats(disks):
    """Read sector counters from /proc/diskstats for *disks*.

    Returns {disk_name: (read_sectors, write_sectors)}.
    """
    result = {}
    disk_set = set(disks)
    with open("/proc/diskstats", "r") as f:
        for line in f:
            parts = line.split()
            if len(parts) >= 14:
                name = parts[2]
                if name in disk_set:
                    result[name] = (int(parts[5]), int(parts[9]))
    return result


def read_netdev(networks):
    """Read byte counters from /proc/net/dev for *networks*.

    Returns {iface: (rx_bytes, tx_bytes)}.
    """
    result = {}
    net_set = set(networks)
    with open("/proc/net/dev", "r") as f:
        for line in f:
            line = line.strip()
            if ":" in line:
                iface, data = line.split(":", 1)
                iface = iface.strip()
                if iface in net_set:
                    parts = data.split()
                    result[iface] = (int(parts[0]), int(parts[8]))
    return result


# ---------------------------------------------------------------------------
# Image rendering
# ---------------------------------------------------------------------------

def parse_color(color_str):
    """Parse '#RRGGBB' into an (R, G, B) tuple."""
    s = str(color_str).strip().lstrip("#")
    return (int(s[0:2], 16), int(s[2:4], 16), int(s[4:6], 16))


def render_image(config, history):
    """Render the indicator image and return a PIL Image (RGB)."""
    width = config["width_px"]
    row_height = config["height_px"]
    border = config["border_px"]
    separator = config["separator_px"]
    latest_value_px = config["latest_value_px"]

    bg_color = parse_color(config["background_color"])
    border_color = parse_color(config["border_color"])
    separator_color = parse_color(config["separator_color"])
    text_color = parse_color(config["text_color"])

    num_rows = 6
    total_height = 2 * border + num_rows * row_height + (num_rows - 1) * separator

    img = Image.new("RGB", (width, total_height), bg_color)
    draw = ImageDraw.Draw(img)

    # --- borders ---
    if border > 0:
        # top
        draw.rectangle([0, 0, width - 1, border - 1], fill=border_color)
        # bottom
        draw.rectangle([0, total_height - border, width - 1, total_height - 1], fill=border_color)
        # left
        draw.rectangle([0, 0, border - 1, total_height - 1], fill=border_color)
        # right
        draw.rectangle([width - border, 0, width - 1, total_height - 1], fill=border_color)

    # --- separators ---
    for i in range(1, num_rows):
        sep_y = border + i * row_height + (i - 1) * separator
        if separator > 0:
            draw.rectangle(
                [border, sep_y, width - border - 1, sep_y + separator - 1],
                fill=separator_color,
            )

    # --- per-row rendering ---
    content_width = max(0, width - 2 * border)
    history_width = max(0, content_width - latest_value_px)

    row_configs = [
        ("cpu",          config["cpu_color"],              100.0),
        ("memory",       config["memory_color"],           100.0),
        ("disk_read",    config["disk_read_color"],        config["disk_read_max_mib_s"]),
        ("disk_write",   config["disk_write_color"],       config["disk_write_max_mib_s"]),
        ("net_download", config["network_download_color"], config["network_download_max_mib_s"]),
        ("net_upload",   config["network_upload_color"],   config["network_upload_max_mib_s"]),
    ]

    # Load font
    try:
        font = ImageFont.truetype(config["text_font_family"], config["text_font_size"])
    except (IOError, OSError):
        font = ImageFont.load_default()

    for row_idx, (key, color_str, max_val) in enumerate(row_configs):
        color = parse_color(color_str)
        row_y = border + row_idx * (row_height + separator)
        values = list(history.get(key, []))

        # History section (left part of content area)
        num_hist = min(len(values), history_width)
        for i in range(num_hist):
            val = values[len(values) - num_hist + i]
            ratio = min(val / max_val, 1.0) if max_val > 0 else 0.0
            bar_h = max(1, int(ratio * row_height)) if ratio > 0 else 0
            x = border + (history_width - num_hist) + i
            if bar_h > 0:
                draw.line(
                    [(x, row_y + row_height - bar_h), (x, row_y + row_height - 1)],
                    fill=color,
                )

        # Latest-value section (right part of content area)
        if latest_value_px > 0 and len(values) > 0:
            latest_val = values[-1]
            ratio = min(latest_val / max_val, 1.0) if max_val > 0 else 0.0
            bar_h = max(1, int(ratio * row_height)) if ratio > 0 else 0
            for px in range(latest_value_px):
                x = border + history_width + px
                if x < width - border and bar_h > 0:
                    draw.line(
                        [(x, row_y + row_height - bar_h), (x, row_y + row_height - 1)],
                        fill=color,
                    )

        # Text overlay (current value, left-aligned)
        if len(values) > 0:
            text = str(int(round(values[-1])))
            text_x = border + config["text_left_padding_px"]
            bbox = font.getbbox(text)
            text_h = bbox[3] - bbox[1]
            text_y = row_y + (row_height - text_h) // 2 - bbox[1]
            draw.text((text_x, text_y), text, fill=text_color, font=font)

    return img


def image_to_ppm_p6(img):
    """Convert a PIL RGB Image to PPM P6 binary bytes."""
    w, h = img.size
    header = f"P6\n{w} {h}\n255\n".encode("ascii")
    return header + img.tobytes()


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    # Ensure work directory exists
    os.makedirs(WORK_DIR, exist_ok=True)

    # Open (or create) the shared lock file; keep fd open for lifetime
    lock_fd = open(LOCK_FILE, "w")

    # --- Check if another DAEMON is already running ---
    content = read_locked(lock_fd, DAEMON_RUN)
    if content:
        parts = content.strip().split()
        if len(parts) == 2:
            pid_str, start_time = parts
            if is_process_running(int(pid_str), start_time):
                lock_fd.close()
                sys.exit(0)

    # Register ourselves
    my_pid = os.getpid()
    my_start = get_process_start_time(my_pid)
    write_locked(lock_fd, DAEMON_RUN, f"{my_pid} {my_start}\n")

    # --- Load config ---
    with open(CONFIG_PATH, "r") as f:
        config = yaml.safe_load(f)
    config_mtime = os.path.getmtime(CONFIG_PATH)

    tick_s = config["tick_ms"] / 1000.0
    gap_s = config["genmon_after_daemon_min_gap_ms"] / 1000.0
    inactive_s = config["daemon_exit_if_genmon_inactive_sec"]

    content_width = max(0, config["width_px"] - 2 * config["border_px"])
    disks = config.get("disks", [])
    networks = config.get("networks", [])

    # --- History ring buffers ---
    history = {
        "cpu":          collections.deque(maxlen=content_width),
        "memory":       collections.deque(maxlen=content_width),
        "disk_read":    collections.deque(maxlen=content_width),
        "disk_write":   collections.deque(maxlen=content_width),
        "net_download": collections.deque(maxlen=content_width),
        "net_upload":   collections.deque(maxlen=content_width),
    }

    # --- Initial /proc snapshots (for diff-based metrics) ---
    prev_cpu = read_cpu()
    prev_disk = read_diskstats(disks)
    prev_net = read_netdev(networks)
    prev_time = time.monotonic()

    last_genmon_timestamp = None
    first_iteration = True

    # ===================================================================
    # Main loop
    # ===================================================================
    while True:
        # --- Read GENMON.out ---
        genmon_content = read_locked(lock_fd, GENMON_OUT)
        genmon_data = parse_kv(genmon_content)

        genmon_ts_str = genmon_data.get("timestamp")
        genmon_ts = float(genmon_ts_str) if genmon_ts_str else None

        # --- Timestamp sanity check ---
        if genmon_ts is not None:
            if last_genmon_timestamp is not None and genmon_ts < last_genmon_timestamp:
                # System clock may have been set backward → exit
                lock_fd.close()
                sys.exit(0)
            last_genmon_timestamp = genmon_ts

            # --- GENMON inactivity check ---
            if time.time() - genmon_ts > inactive_s:
                lock_fd.close()
                sys.exit(0)

        # --- Config file change check ---
        try:
            if os.path.getmtime(CONFIG_PATH) != config_mtime:
                lock_fd.close()
                sys.exit(0)
        except OSError:
            lock_fd.close()
            sys.exit(0)

        # ---------------------------------------------------------------
        # Gather system metrics
        # ---------------------------------------------------------------
        now_mono = time.monotonic()
        dt = now_mono - prev_time

        # CPU (instant ratio – works even on first iteration)
        cur_cpu = read_cpu()
        cpu_total_diff = cur_cpu[0] - prev_cpu[0]
        cpu_idle_diff = cur_cpu[1] - prev_cpu[1]
        cpu_pct = ((1.0 - cpu_idle_diff / cpu_total_diff) * 100.0
                   if cpu_total_diff > 0 else 0.0)
        cpu_pct = max(0.0, min(100.0, cpu_pct))
        prev_cpu = cur_cpu

        # Memory (absolute – no diff needed)
        mem_pct = max(0.0, min(100.0, read_memory()))

        # Disk throughput (diff-based)
        cur_disk = read_diskstats(disks)
        if first_iteration or dt <= 0:
            disk_read_mib_s = 0.0
            disk_write_mib_s = 0.0
        else:
            total_read_sectors = 0
            total_write_sectors = 0
            for disk in disks:
                if disk in cur_disk and disk in prev_disk:
                    total_read_sectors += cur_disk[disk][0] - prev_disk[disk][0]
                    total_write_sectors += cur_disk[disk][1] - prev_disk[disk][1]
            # Sectors are 512 bytes
            disk_read_mib_s = (total_read_sectors * 512.0) / (1024.0 * 1024.0) / dt
            disk_write_mib_s = (total_write_sectors * 512.0) / (1024.0 * 1024.0) / dt
        prev_disk = cur_disk

        # Network throughput (diff-based)
        cur_net = read_netdev(networks)
        if first_iteration or dt <= 0:
            net_download_mib_s = 0.0
            net_upload_mib_s = 0.0
        else:
            total_rx = 0
            total_tx = 0
            for net in networks:
                if net in cur_net and net in prev_net:
                    total_rx += cur_net[net][0] - prev_net[net][0]
                    total_tx += cur_net[net][1] - prev_net[net][1]
            net_download_mib_s = total_rx / (1024.0 * 1024.0) / dt
            net_upload_mib_s = total_tx / (1024.0 * 1024.0) / dt
        prev_net = cur_net
        prev_time = now_mono
        first_iteration = False

        # ---------------------------------------------------------------
        # Update history
        # ---------------------------------------------------------------
        history["cpu"].append(cpu_pct)
        history["memory"].append(mem_pct)
        history["disk_read"].append(disk_read_mib_s)
        history["disk_write"].append(disk_write_mib_s)
        history["net_download"].append(net_download_mib_s)
        history["net_upload"].append(net_upload_mib_s)

        # ---------------------------------------------------------------
        # Choose next image buffer ID (triple-buffer)
        # ---------------------------------------------------------------
        daemon_content = read_locked(lock_fd, DAEMON_OUT)
        daemon_data = parse_kv(daemon_content)
        daemon_img_id = int(daemon_data.get("img_id", -1))
        genmon_img_id = int(genmon_data.get("img_id", -1))

        next_img_id = 0
        for candidate in (0, 1, 2):
            if candidate != daemon_img_id and candidate != genmon_img_id:
                next_img_id = candidate
                break

        # ---------------------------------------------------------------
        # Render & write image
        # ---------------------------------------------------------------
        img = render_image(config, history)
        ppm_data = image_to_ppm_p6(img)
        img_path = os.path.join(WORK_DIR, f"img.{next_img_id}.ppm")
        write_binary_locked(lock_fd, img_path, ppm_data)

        # ---------------------------------------------------------------
        # Write DAEMON.out
        # ---------------------------------------------------------------
        write_locked(lock_fd, DAEMON_OUT, format_kv({
            "timestamp": str(time.time()),
            "img_id": str(next_img_id),
        }))

        # ---------------------------------------------------------------
        # Wait until estimated next GENMON tick + gap
        # ---------------------------------------------------------------
        now_wall = time.time()
        if genmon_ts is not None and genmon_ts > 0:
            elapsed_since_genmon = now_wall - genmon_ts
            if elapsed_since_genmon > 0:
                ticks_elapsed = int(elapsed_since_genmon / tick_s) + 1
            else:
                ticks_elapsed = 1
            next_genmon = genmon_ts + ticks_elapsed * tick_s
            wait_time = (next_genmon + gap_s) - now_wall
        else:
            wait_time = tick_s

        if wait_time > 2 * tick_s:
            # Abnormal delay – exit and let GENMON respawn us
            lock_fd.close()
            sys.exit(0)

        if wait_time > 0:
            time.sleep(wait_time)


if __name__ == "__main__":
    main()
