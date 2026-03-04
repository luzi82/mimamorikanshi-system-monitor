#!/bin/bash -e

# MimamoriKanshi System Monitor - GENMON
#
# Called by xfce4-genmon-plugin.
# - Launches DAEMON if not running
# - Echoes DAEMON-generated image path to xfce4-genmon-plugin

WORK_DIR="/dev/shm/mimamorikanshi"
LOCK_FILE="$WORK_DIR/lock"
GENMON_RUN="$WORK_DIR/GENMON.run"
GENMON_OUT="$WORK_DIR/GENMON.out"
DAEMON_RUN="$WORK_DIR/DAEMON.run"
DAEMON_OUT="$WORK_DIR/DAEMON.out"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

mkdir -p "$WORK_DIR"
touch "$LOCK_FILE"

# Open lock file descriptor (kept for the lifetime of this script)
exec 200<>"$LOCK_FILE"

# ---------------------------------------------------------------------------
# Helper functions
# ---------------------------------------------------------------------------

get_start_time() {
    # Get process start time (field 22) from /proc/<pid>/stat
    awk '{print $22}' "/proc/$1/stat" 2>/dev/null
}

is_process_running() {
    # $1=pid  $2=start_time
    local pid="$1" expected="$2"
    if [[ -d "/proc/$pid" ]]; then
        local actual
        actual=$(get_start_time "$pid")
        [[ "$actual" == "$expected" ]] && return 0
    fi
    return 1
}

read_locked() {
    # Read a file under non-blocking flock.  Prints content to stdout.
    flock -n 200
    local content=""
    if [[ -f "$1" ]]; then
        content=$(cat "$1" 2>/dev/null)
    fi
    flock -u 200
    printf '%s' "$content"
}

write_locked() {
    # Write content to file atomically under non-blocking flock.
    # $1=target_path  $2=content
    printf '%s' "$2" > "${1}.tmp"
    flock -n 200
    mv "${1}.tmp" "$1"
    flock -u 200
}

# ---------------------------------------------------------------------------
# 1. Check if another GENMON instance is already running
# ---------------------------------------------------------------------------

GENMON_RUN_CONTENT=$(read_locked "$GENMON_RUN")
if [[ -n "$GENMON_RUN_CONTENT" ]]; then
    OLD_PID=$(echo "$GENMON_RUN_CONTENT" | awk '{print $1}')
    OLD_START=$(echo "$GENMON_RUN_CONTENT" | awk '{print $2}')
    if is_process_running "$OLD_PID" "$OLD_START"; then
        exec 200>&-
        exit 0
    fi
fi

# Register ourselves
MY_PID=$$
MY_START=$(get_start_time "$MY_PID")
write_locked "$GENMON_RUN" "$MY_PID $MY_START"

# ---------------------------------------------------------------------------
# 2. Check if DAEMON is running
# ---------------------------------------------------------------------------

DAEMON_RUNNING=false
DAEMON_RUN_CONTENT=$(read_locked "$DAEMON_RUN")
if [[ -n "$DAEMON_RUN_CONTENT" ]]; then
    DAEMON_PID=$(echo "$DAEMON_RUN_CONTENT" | awk '{print $1}')
    DAEMON_START=$(echo "$DAEMON_RUN_CONTENT" | awk '{print $2}')
    if is_process_running "$DAEMON_PID" "$DAEMON_START"; then
        DAEMON_RUNNING=true
    fi
fi

TIMESTAMP=$(date +%s.%N)

# ---------------------------------------------------------------------------
# 3. No running DAEMON → launch it, show loading text
# ---------------------------------------------------------------------------

if [[ "$DAEMON_RUNNING" == "false" ]]; then
    write_locked "$GENMON_OUT" "timestamp=${TIMESTAMP}
img_id=-1
"
    # Launch DAEMON in background
    python3 "$SCRIPT_DIR/daemon.py" &
    disown

    echo "<txt>Loading...</txt>"
    exec 200>&-
    exit 0
fi

# ---------------------------------------------------------------------------
# 4. DAEMON is active → echo its last generated image
# ---------------------------------------------------------------------------

DAEMON_OUT_CONTENT=$(read_locked "$DAEMON_OUT")
IMG_ID=$(echo "$DAEMON_OUT_CONTENT" | grep "^img_id=" | cut -d= -f2)

if [[ -n "$IMG_ID" && "$IMG_ID" != "-1" ]]; then
    IMG_PATH="$WORK_DIR/img.${IMG_ID}.ppm"
    echo "<img>$IMG_PATH</img>"

    write_locked "$GENMON_OUT" "timestamp=${TIMESTAMP}
img_id=${IMG_ID}
"
else
    # DAEMON started but hasn't produced an image yet
    echo "<txt>Loading...</txt>"

    write_locked "$GENMON_OUT" "timestamp=${TIMESTAMP}
img_id=-1
"
fi

exec 200>&-
exit 0
