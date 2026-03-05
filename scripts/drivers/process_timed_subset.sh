#!/usr/bin/env bash
# process_timed_subset.sh — Run process_nspe_in_files.sh on timing-filtered input files
#
# Reads a _timing.log to identify input files whose mean runtime falls below a
# specified threshold, then delegates to process_nspe_in_files.sh with those
# files selected via --pattern arguments.
#
# Usage:
#   process_timed_subset.sh --timing-log <file> --max-time <secs> <mode> [directory] [options]
#
# Required:
#   --timing-log <file>   Path to _timing.log (TSV from process_nspe_in_files.sh)
#   --max-time <secs>     Include only files with mean runtime strictly below this value
#
# Additional options are passed through directly to process_nspe_in_files.sh.
# See process_nspe_in_files.sh --help for the full list.
#
# Examples:
#   # Run make mode on all cases that completed in under 5 seconds
#   process_timed_subset.sh --timing-log std_new/_timing.log --max-time 5 make std_new
#
#   # Test mode, under 10 seconds, with a specific executable
#   process_timed_subset.sh --timing-log std_new/_timing.log --max-time 10 test std_new --exe ./nspe.x
#
#   # Dry run to preview which files would be selected
#   process_timed_subset.sh --timing-log std_new/_timing.log --max-time 5 make std_new --dry-run

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TIMING_STATS="$SCRIPT_DIR/timing_stats.sh"
PROCESS_SCRIPT="$SCRIPT_DIR/process_nspe_in_files.sh"

usage() {
    sed -n '2,/^$/{ s/^# \?//; p }' "$0"
    exit "${1:-0}"
}

# ─── Argument parsing ────────────────────────────────────────────────────────

timing_log=""
max_time=""
passthrough_args=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --timing-log) timing_log="${2:?Error: --timing-log requires a value}"; shift 2 ;;
        --max-time)   max_time="${2:?Error: --max-time requires a value}";     shift 2 ;;
        --help|-h)    usage 0 ;;
        *)            passthrough_args+=("$1"); shift ;;
    esac
done

if [[ -z "$timing_log" || -z "$max_time" ]]; then
    echo "Error: --timing-log and --max-time are required." >&2
    usage 1
fi

if [[ ! -f "$timing_log" ]]; then
    echo "Error: Timing log not found: $timing_log" >&2
    exit 1
fi

if ! [[ "$max_time" =~ ^[0-9]+([.][0-9]+)?$ ]]; then
    echo "Error: --max-time must be a positive number (got: $max_time)" >&2
    exit 1
fi

# ─── Filter by timing threshold ──────────────────────────────────────────────

mapfile -t files < <("$TIMING_STATS" "$timing_log" --list-below "$max_time")

if [[ ${#files[@]} -eq 0 ]]; then
    echo "No files found with mean runtime below ${max_time}s in: $timing_log" >&2
    exit 1
fi

echo "Timing filter: mean < ${max_time}s → ${#files[@]} file(s) selected from $timing_log"

# ─── Build --pattern arguments and delegate ──────────────────────────────────

pattern_args=()
for f in "${files[@]}"; do
    [[ -n "$f" ]] && pattern_args+=(--pattern "$f")
done

exec "$PROCESS_SCRIPT" "${passthrough_args[@]}" "${pattern_args[@]}"
