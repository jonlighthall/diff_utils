#!/usr/bin/env bash
# timing_stats.sh — Statistical summary of _timing.log files
#
# Reads one or more _timing.log files (TSV format from process_nspe_in_files.sh)
# and produces per-input-file statistics: count, mean, std dev, min, max.
#
# Usage:
#   timing_stats.sh <_timing.log> [_timing.log ...]
#   timing_stats.sh <_timing.log> --exe <executable> --status PASS --host <hostname>
#   timing_stats.sh dir1/_timing.log dir2/_timing.log --compare
#
# Options:
#   --exe <name>       Filter by executable basename (e.g., nspe.x)
#   --status <s>       Filter by status (default: PASS; use FAIL, TIMEOUT, etc.)
#   --all              Include all statuses (PASS, FAIL, TIMEOUT)
#   --host <name>      Filter by hostname
#   --compare          Side-by-side comparison of two _timing.log files
#   --csv              Output in CSV format instead of table
#   --sort <col>       Sort by: name (default), mean, count, stddev, min, max, ratio, sigma (compare mode)
#   --list-below <N>   Output only filenames where mean runtime < N seconds (one per line)
#   --help             Show this help message
#
# Examples:
#   # Summarize a single file
#   timing_stats.sh std_copy/_timing.log
#
#   # Compare Fortran vs C++ timing
#   timing_stats.sh fortran/std_copy/_timing.log cpp/std_copy/_timing.log --compare
#
#   # Include failed and timed-out runs
#   timing_stats.sh _timing.log --all
#
#   # Show only timed-out runs
#   timing_stats.sh _timing.log --status TIMEOUT

set -euo pipefail

# ─── Argument parsing ───────────────────────────────────────────────────────

declare -a log_files=()
filter_exe=""
filter_status="PASS"
filter_host=""
compare_mode=false
csv_mode=false
sort_col="name"
list_below=""

usage() {
    sed -n '2,/^$/{ s/^# \?//; p }' "$0"
    exit "${1:-0}"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --exe)      filter_exe="${2:?Error: --exe requires a value}";       shift 2 ;;
        --status)   filter_status="${2:?Error: --status requires a value}"; shift 2 ;;
        --all)      filter_status="";   shift   ;;
        --host)     filter_host="${2:?Error: --host requires a value}";     shift 2 ;;
        --compare)   compare_mode=true;  shift   ;;
        --csv)       csv_mode=true;      shift   ;;
        --list-below) list_below="${2:?Error: --list-below requires a value}"; shift 2 ;;
        --sort)
            if [[ -z "${2:-}" || "$2" == -* ]]; then
                sort_col="mean"
                shift
            else
                sort_col="$2"
                shift 2
            fi
            ;;
        --help|-h)  usage 0 ;;
        -*)         echo "Unknown option: $1" >&2; usage 1 ;;
        *)          log_files+=("$1");  shift   ;;
    esac
done

if [[ ${#log_files[@]} -eq 0 ]]; then
    echo "Error: No timing.log file(s) specified." >&2
    usage 1
fi

for f in "${log_files[@]}"; do
    if [[ ! -f "$f" ]]; then
        echo "Error: File not found: $f" >&2
        exit 1
    fi
done

if $compare_mode && [[ ${#log_files[@]} -ne 2 ]]; then
    echo "Error: --compare requires exactly 2 timing.log files." >&2
    exit 1
fi

# ─── AWK statistics engine ──────────────────────────────────────────────────

# Build AWK filter conditions
awk_filter=""
if [[ -n "$filter_exe" ]]; then
    awk_filter="${awk_filter} && \$4 == \"${filter_exe}\""
fi
if [[ -n "$filter_status" ]]; then
    awk_filter="${awk_filter} && \$6 == \"${filter_status}\""
fi
if [[ -n "$filter_host" ]]; then
    awk_filter="${awk_filter} && \$1 == \"${filter_host}\""
fi
# Remove leading " && "
awk_filter="${awk_filter# && }"
# Default to "1" (match all) if empty
awk_filter="${awk_filter:-1}"

# Map sort column name to index for AWK
case "$sort_col" in
    name)   sort_key="name"   ;;
    mean)   sort_key="mean"   ;;
    count)  sort_key="count"  ;;
    stddev) sort_key="stddev" ;;
    min)    sort_key="min"    ;;
    max)    sort_key="max"    ;;
    ratio)  sort_key="ratio"
            if ! $compare_mode; then
                echo "Error: --sort ratio is only valid with --compare" >&2; exit 1
            fi ;;
    sigma)  sort_key="sigma"
            if ! $compare_mode; then
                echo "Error: --sort sigma is only valid with --compare" >&2; exit 1
            fi ;;
    *)      echo "Error: Unknown sort column: $sort_col" >&2; exit 1 ;;
esac

compute_stats() {
    # Reads a timing.log from stdin, outputs TSV: input_file, count, mean, stddev, min, max
    awk -F'\t' -v filter="$awk_filter" -v sort_key="$sort_key" '
    BEGIN { OFS = "\t" }
    NR == 1 { next }  # skip header
    {
        # Apply filter
        if (!('"$awk_filter"')) next

        f = $5  # input_file
        t = $7 + 0  # elapsed_s (force numeric)
        n[f]++
        sum[f] += t
        sumsq[f] += t * t
        if (!(f in mn) || t < mn[f]) mn[f] = t
        if (!(f in mx) || t > mx[f]) mx[f] = t
        # Preserve insertion order
        if (!(f in seen)) {
            seen[f] = 1
            order[++nfiles] = f
        }
    }
    END {
        # Collect all results into arrays for sorting
        for (i = 1; i <= nfiles; i++) {
            f = order[i]
            mean = sum[f] / n[f]
            if (n[f] > 1) {
                variance = (sumsq[f] - n[f] * mean * mean) / (n[f] - 1)
                if (variance < 0) variance = 0  # guard for floating point
                sd = sqrt(variance)
            } else {
                sd = 0
            }
            names[i]   = f
            counts[i]  = n[f]
            means[i]   = mean
            stddevs[i] = sd
            mins[i]    = mn[f]
            maxs[i]    = mx[f]
        }

        # Simple insertion sort (stable, fine for typical file counts)
        for (i = 2; i <= nfiles; i++) {
            j = i
            while (j > 1) {
                swap = 0
                if (sort_key == "mean"   && means[j]   < means[j-1])   swap = 1
                if (sort_key == "count"  && counts[j]   < counts[j-1])  swap = 1
                if (sort_key == "stddev" && stddevs[j] < stddevs[j-1]) swap = 1
                if (sort_key == "min"    && mins[j]    < mins[j-1])    swap = 1
                if (sort_key == "max"    && maxs[j]    < maxs[j-1])    swap = 1
                if (sort_key == "name"   && names[j]   < names[j-1])   swap = 1
                if (!swap) break
                # Swap all arrays
                tmp = names[j];   names[j]   = names[j-1];   names[j-1]   = tmp
                tmp = counts[j];  counts[j]  = counts[j-1];  counts[j-1]  = tmp
                tmp = means[j];   means[j]   = means[j-1];   means[j-1]   = tmp
                tmp = stddevs[j]; stddevs[j] = stddevs[j-1]; stddevs[j-1] = tmp
                tmp = mins[j];    mins[j]    = mins[j-1];    mins[j-1]    = tmp
                tmp = maxs[j];    maxs[j]    = maxs[j-1];    maxs[j-1]    = tmp
                j--
            }
        }

        # Compute totals
        total_n = 0; total_min = -1; total_max = 0
        for (i = 1; i <= nfiles; i++) {
            total_n += counts[i]
            if (total_min < 0 || mins[i] < total_min) total_min = mins[i]
            if (maxs[i] > total_max) total_max = maxs[i]
        }

        # Output sorted results
        for (i = 1; i <= nfiles; i++) {
            printf "%s\t%d\t%.6f\t%.6f\t%.6f\t%.6f\n", \
                names[i], counts[i], means[i], stddevs[i], mins[i], maxs[i]
        }
        # Total line
        if (nfiles > 0 && total_n > 0)
            printf "TOTAL\t%d\t-\t-\t%.6f\t%.6f\n", total_n, total_min, total_max
    }'
}

# ─── Single-file summary mode ───────────────────────────────────────────────

format_table() {
    # Reads TSV from stdin with columns: name, count, mean, stddev, min, max
    # Formats as an aligned table
    local header="input_file	n	mean (s)	std dev	min (s)	max (s)"
    {
        echo "$header"
        cat
    } | column -t -s$'\t'
}

format_csv() {
    local header="input_file,n,mean_s,stddev_s,min_s,max_s"
    echo "$header"
    sed 's/\t/,/g'
}

if ! $compare_mode; then
    # Single-file or multi-file merged summary
    echo "Timing Statistics"
    echo "═══════════════════════════════════════════════════════════════════"
    for f in "${log_files[@]}"; do
        echo "  Source: $f"
    done
    total_records=$(cat "${log_files[@]}" | grep -cv '^hostname' || true)
    echo "  Records: $total_records"
    if [[ -n "$filter_exe" ]]; then
        echo "  Filter executable: $filter_exe"
    fi
    if [[ -n "$filter_status" ]]; then
        echo "  Filter status: $filter_status"
    fi
    if [[ -n "$filter_host" ]]; then
        echo "  Filter host: $filter_host"
    fi
    echo "  Sort by: $sort_col"
    echo "═══════════════════════════════════════════════════════════════════"
    echo

    stats_output=$(cat "${log_files[@]}" | compute_stats)

    if [[ -z "$stats_output" ]]; then
        echo "No matching records found."
        exit 0
    fi

    # --list-below: emit only filenames with mean < threshold; skip TOTAL
    if [[ -n "$list_below" ]]; then
        echo "$stats_output" | awk -F'\t' -v thresh="$list_below" \
            '$1 != "TOTAL" && $3 != "-" && $3+0 < thresh+0 { print $1 }'
        exit 0
    fi

    if $csv_mode; then
        echo "$stats_output" | format_csv
    else
        echo "$stats_output" | format_table
    fi
    exit 0
fi

# ─── Compare mode ───────────────────────────────────────────────────────────

echo "Timing Comparison"
echo "═══════════════════════════════════════════════════════════════════"
echo "  A: ${log_files[0]}"
echo "  B: ${log_files[1]}"
if [[ -n "$filter_exe" ]]; then
    echo "  Filter executable: $filter_exe"
fi
if [[ -n "$filter_status" ]]; then
    echo "  Filter status: $filter_status"
fi
echo "═══════════════════════════════════════════════════════════════════"
echo

# Compute stats for both files
stats_a=$(compute_stats < "${log_files[0]}")
stats_b=$(compute_stats < "${log_files[1]}")

# Join on input_file and produce side-by-side comparison
paste <(echo "$stats_a") <(echo "$stats_b") | awk -F'\t' -v sort_key="$sort_key" '
{
    # From file A: $1=name, $2=count, $3=mean, $4=stddev, $5=min, $6=max
    # From file B: $7=name, $8=count, $9=mean, $10=stddev, $11=min, $12=max
    name_a = $1; n_a = $2; mean_a = $3; sd_a = $4
    name_b = $7; n_b = $8; mean_b = $9; sd_b = $10

    # Use whichever name is available
    name = (name_a != "" ? name_a : name_b)
    if (name == "TOTAL") next  # skip individual totals

    nrows++
    r_name[nrows]   = name
    r_n_a[nrows]    = (n_a != "" ? n_a : "-")
    r_mean_a[nrows] = (mean_a != "" ? mean_a + 0 : 0)
    r_sd_a[nrows]   = (sd_a != "" ? sd_a + 0 : 0)
    r_n_b[nrows]    = (n_b != "" ? n_b : "-")
    r_mean_b[nrows] = (mean_b != "" ? mean_b + 0 : 0)
    r_sd_b[nrows]   = (sd_b != "" ? sd_b + 0 : 0)

    if (mean_a + 0 > 0 && mean_b + 0 > 0)
        r_ratio[nrows] = (mean_b + 0) / (mean_a + 0)
    else
        r_ratio[nrows] = 9999  # sort missing ratios to end

    # Sigma: Welch t-statistic — how many std errors apart
    se2 = 0
    if (n_a + 0 > 1 && sd_a + 0 > 0) se2 += (sd_a * sd_a) / n_a
    if (n_b + 0 > 1 && sd_b + 0 > 0) se2 += (sd_b * sd_b) / n_b
    if (se2 > 0)
        r_sigma[nrows] = (r_mean_b[nrows] - r_mean_a[nrows]) / sqrt(se2)
    else
        r_sigma[nrows] = 0
}
END {
    # Sort if requested
    for (i = 2; i <= nrows; i++) {
        j = i
        while (j > 1) {
            swap = 0
            if (sort_key == "ratio"  && r_ratio[j]  < r_ratio[j-1])  swap = 1
            if (sort_key == "sigma"  && r_sigma[j]  < r_sigma[j-1])  swap = 1
            if (sort_key == "mean"   && r_mean_a[j]  < r_mean_a[j-1]) swap = 1
            if (sort_key == "name"   && r_name[j]   < r_name[j-1])   swap = 1
            if (!swap) break
            # Swap all arrays
            tmp = r_name[j];   r_name[j]   = r_name[j-1];   r_name[j-1]   = tmp
            tmp = r_n_a[j];    r_n_a[j]    = r_n_a[j-1];    r_n_a[j-1]    = tmp
            tmp = r_mean_a[j]; r_mean_a[j] = r_mean_a[j-1]; r_mean_a[j-1] = tmp
            tmp = r_sd_a[j];   r_sd_a[j]   = r_sd_a[j-1];   r_sd_a[j-1]   = tmp
            tmp = r_n_b[j];    r_n_b[j]    = r_n_b[j-1];    r_n_b[j-1]    = tmp
            tmp = r_mean_b[j]; r_mean_b[j] = r_mean_b[j-1]; r_mean_b[j-1] = tmp
            tmp = r_sd_b[j];   r_sd_b[j]   = r_sd_b[j-1];   r_sd_b[j-1]   = tmp
            tmp = r_ratio[j];  r_ratio[j]  = r_ratio[j-1];  r_ratio[j-1]  = tmp
            tmp = r_sigma[j];  r_sigma[j]  = r_sigma[j-1];  r_sigma[j-1]  = tmp
            j--
        }
    }

    # Print header
    printf "%-20s  %4s  %10s %10s  %4s  %10s %10s  %8s  %7s\n", \
        "input_file", "n_A", "mean_A (s)", "stddev_A", "n_B", "mean_B (s)", "stddev_B", "ratio B/A", "sigma"
    printf "%-20s  %4s  %10s %10s  %4s  %10s %10s  %8s  %7s\n", \
        "--------------------", "----", "----------", "----------", "----", "----------", "----------", "--------", "-------"

    # Print rows
    for (i = 1; i <= nrows; i++) {
        if (r_ratio[i] < 9999) {
            ratio_s = sprintf("%.3f", r_ratio[i])
            if (r_ratio[i] < 1.0)
                ratio_str = sprintf("\033[32m%8s\033[0m", ratio_s)
            else if (r_ratio[i] > 1.0)
                ratio_str = sprintf("\033[31m%8s\033[0m", ratio_s)
            else
                ratio_str = sprintf("%8s", ratio_s)
        } else {
            ratio_str = sprintf("%8s", "-")
        }

        # Format sigma with color: >|2| yellow, >|3| red/green
        if (r_sigma[i] != 0) {
            sigma_s = sprintf("%.1f", r_sigma[i])
            abs_sig = (r_sigma[i] < 0 ? -r_sigma[i] : r_sigma[i])
            if (abs_sig >= 3.0) {
                if (r_sigma[i] > 0)
                    sigma_str = sprintf("\033[31m%7s\033[0m", sigma_s)
                else
                    sigma_str = sprintf("\033[32m%7s\033[0m", sigma_s)
            } else if (abs_sig >= 2.0) {
                sigma_str = sprintf("\033[33m%7s\033[0m", sigma_s)
            } else {
                sigma_str = sprintf("%7s", sigma_s)
            }
        } else {
            sigma_str = sprintf("%7s", "-")
        }

        printf "%-20s  %4s  %10s %10s  %4s  %10s %10s  %s  %s\n", \
            r_name[i], \
            r_n_a[i], \
            (r_mean_a[i] > 0 ? sprintf("%.6f", r_mean_a[i]) : "-"), \
            (r_sd_a[i] > 0 ? sprintf("%.6f", r_sd_a[i]) : "-"), \
            r_n_b[i], \
            (r_mean_b[i] > 0 ? sprintf("%.6f", r_mean_b[i]) : "-"), \
            (r_sd_b[i] > 0 ? sprintf("%.6f", r_sd_b[i]) : "-"), \
            ratio_str, \
            sigma_str
    }
}
'
