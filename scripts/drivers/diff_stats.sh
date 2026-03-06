#!/usr/bin/env bash
# diff_stats.sh — Summary analysis of _diff.log files
#
# Reads one or more _diff.log files (TSV format from process_nspe_in_files.sh)
# and produces a per-basename summary showing PASS, FAIL, or MIXED status.
# A basename is MIXED if it has both PASS and FAIL entries across its files
# (e.g., case1r_01.asc PASS + case1r_02.asc FAIL → case1r MIXED).
#
# Expected _diff.log columns (11-column format):
#   hostname  date  time  test_file  ref_file  status  method  max_err  n_sig  n_nz  rmse
# Backward-compatible with older 6/10-column formats (missing columns shown as "-").
#
# Usage:
#   diff_stats.sh <_diff.log> [_diff.log ...]
#   diff_stats.sh <_diff.log> --status FAIL
#   diff_stats.sh <_diff.log> --status MIXED
#   diff_stats.sh <_diff.log> --all
#
# Options:
#   --status <s>       Filter by derived status: PASS, FAIL, MIXED (default: show FAIL+MIXED)
#   --all              Show all basenames regardless of status
#   --host <name>      Filter by hostname
#   --csv              Output in CSV format instead of table
#   --sort <col>       Sort by: name (default), status, pass, fail, total
#   --files            Show per-file detail instead of per-basename summary
#   --help             Show this help message
#
# Examples:
#   # Default: show overview + FAIL/MIXED basenames
#   diff_stats.sh std_new/_diff.log
#
#   # Show all basenames with status
#   diff_stats.sh std_new/_diff.log --all
#
#   # Show only MIXED basenames
#   diff_stats.sh std_new/_diff.log --status MIXED
#
#   # Show only FAILed basenames
#   diff_stats.sh std_new/_diff.log --status FAIL
#
#   # Per-file detail
#   diff_stats.sh std_new/_diff.log --files --status FAIL

set -euo pipefail

# ─── Argument parsing ───────────────────────────────────────────────────────

declare -a log_files=()
filter_status=""
filter_host=""
csv_mode=false
sort_col="name"
show_files=false
show_all=false

usage() {
    sed -n '2,/^$/{ s/^# \?//; p }' "$0"
    exit "${1:-0}"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --status)   filter_status="${2:?Error: --status requires a value}"; shift 2 ;;
        --all)      show_all=true;      shift   ;;
        --host)     filter_host="${2:?Error: --host requires a value}";     shift 2 ;;
        --csv)      csv_mode=true;      shift   ;;
        --sort)
            if [[ -z "${2:-}" || "$2" == -* ]]; then
                sort_col="status"
                shift
            else
                sort_col="$2"
                shift 2
            fi
            ;;
        --files)    show_files=true;    shift   ;;
        --help|-h)  usage 0 ;;
        -*)         echo "Unknown option: $1" >&2; usage 1 ;;
        *)          log_files+=("$1");  shift   ;;
    esac
done

if [[ ${#log_files[@]} -eq 0 ]]; then
    echo "Error: No _diff.log file(s) specified." >&2
    usage 1
fi

for f in "${log_files[@]}"; do
    if [[ ! -f "$f" ]]; then
        echo "Error: File not found: $f" >&2
        exit 1
    fi
done

# Validate sort column
case "$sort_col" in
    name|status|pass|fail|total) ;;
    *) echo "Error: Unknown sort column: $sort_col (valid: name, status, pass, fail, total)" >&2; exit 1 ;;
esac

# Validate filter status
if [[ -n "$filter_status" ]]; then
    case "$filter_status" in
        PASS|FAIL|MIXED) ;;
        *) echo "Error: Unknown status filter: $filter_status (valid: PASS, FAIL, MIXED)" >&2; exit 1 ;;
    esac
fi

# ─── AWK analysis engine ────────────────────────────────────────────────────

awk -F'\t' \
    -v filter_status="$filter_status" \
    -v filter_host="$filter_host" \
    -v csv_mode="$csv_mode" \
    -v sort_col="$sort_col" \
    -v show_files="$show_files" \
    -v show_all="$show_all" \
'
# ─── Helper: extract basename from test_file ─────────────────────────────
# Strip _01.asc, _02.asc, _03.asc, .tl, .rtl, .ftl suffixes
function get_basename(fname,    bn) {
    bn = fname
    if (match(bn, /_0[1-9]\.asc$/)) {
        bn = substr(bn, 1, RSTART - 1)
    } else if (match(bn, /\.tl$/)) {
        bn = substr(bn, 1, RSTART - 1)
    } else if (match(bn, /\.rtl$/)) {
        bn = substr(bn, 1, RSTART - 1)
    } else if (match(bn, /\.ftl$/)) {
        bn = substr(bn, 1, RSTART - 1)
    }
    return bn
}

# ─── Pass 1: read all records ────────────────────────────────────────────
NR == 1 { next }  # skip header(s)
/^hostname\t/ { next }  # skip headers from concatenated files

{
    # Apply host filter
    if (filter_host != "" && $1 != filter_host) next

    test_file = $4
    status = $6
    method = ($7 != "" ? $7 : "-")
    max_err = ($8 != "" ? $8 : "-")
    n_sig   = ($9 != "" ? $9 : "-")
    n_nz    = ($10 != "" ? $10 : "-")
    rmse    = ($11 != "" ? $11 : "-")
    total_records++

    if (status == "PASS") total_pass++
    else if (status == "FAIL") total_fail++

    # Track method distribution (overall)
    if (method != "-") method_count[method]++

    # Track unique files
    if (!(test_file in file_seen)) {
        file_seen[test_file] = 1
        unique_files++
    }

    # Track per-file status and method (for --files mode)
    file_status[test_file] = status  # last status wins for duplicates
    file_method[test_file] = method
    file_max_err[test_file] = max_err
    file_rmse[test_file] = rmse

    # Track per-basename
    bn = get_basename(test_file)
    if (!(bn in bn_seen)) {
        bn_seen[bn] = 1
        bn_order[++n_basenames] = bn
    }
    if (status == "PASS") bn_pass[bn]++
    if (status == "FAIL") bn_fail[bn]++
    bn_total[bn]++

    # Track per-basename method distribution
    if (method != "-") {
        bn_method_count[bn, method]++
        if (!(bn in bn_methods_list)) {
            bn_methods_list[bn] = method
        } else if (index(bn_methods_list[bn], method) == 0) {
            bn_methods_list[bn] = bn_methods_list[bn] "," method
        }
    }

    # Track per-basename max error (keep the maximum)
    if (max_err != "-" && max_err + 0 > 0) {
        if (!(bn in bn_max_err) || max_err + 0 > bn_max_err[bn] + 0) {
            bn_max_err[bn] = max_err
        }
    }

    # Track per-basename max RMSE (keep the maximum)
    if (rmse != "-" && rmse + 0 > 0) {
        if (!(bn in bn_max_rmse) || rmse + 0 > bn_max_rmse[bn] + 0) {
            bn_max_rmse[bn] = rmse
        }
    }
}

# ─── Pass 2: compute derived status and output ──────────────────────────
END {
    if (total_records == 0) {
        print "No records found."
        exit 0
    }

    # Compute derived status for each basename
    n_pass = 0; n_fail = 0; n_mixed = 0
    for (i = 1; i <= n_basenames; i++) {
        bn = bn_order[i]
        has_pass = (bn_pass[bn] + 0 > 0)
        has_fail = (bn_fail[bn] + 0 > 0)
        if (has_pass && has_fail) {
            bn_status[bn] = "MIXED"
            n_mixed++
        } else if (has_fail) {
            bn_status[bn] = "FAIL"
            n_fail++
        } else {
            bn_status[bn] = "PASS"
            n_pass++
        }
    }

    # ─── Per-file mode ───────────────────────────────────────────────────
    if (show_files == "true") {
        if (csv_mode == "true") {
            print "file,status,method,max_err,rmse"
        } else {
            printf "%-60s %-6s %-8s %-10s %s\n", "FILE", "STATUS", "METHOD", "MAX_ERR", "RMSE"
            for (j = 1; j <= 95; j++) printf "-"
            print ""
        }
        # Sort file names
        n_fkeys = 0
        for (f in file_seen) fkeys[++n_fkeys] = f
        # Simple insertion sort
        for (a = 2; a <= n_fkeys; a++) {
            key = fkeys[a]
            b = a - 1
            while (b > 0 && fkeys[b] > key) {
                fkeys[b + 1] = fkeys[b]
                b--
            }
            fkeys[b + 1] = key
        }
        count = 0
        for (j = 1; j <= n_fkeys; j++) {
            f = fkeys[j]
            s = file_status[f]
            if (filter_status != "" && s != filter_status) continue
            m = file_method[f]
            e = file_max_err[f]
            r = (f in file_rmse ? file_rmse[f] : "-")
            if (csv_mode == "true") {
                printf "%s,%s,%s,%s,%s\n", f, s, m, e, r
            } else {
                if (s == "FAIL") {
                    printf "%-60s \033[1;31m%-6s\033[0m %-8s %-10s %s\n", f, s, m, e, r
                } else {
                    printf "%-60s \033[1;32m%-6s\033[0m %-8s %-10s %s\n", f, s, m, e, r
                }
            }
            count++
        }
        if (csv_mode != "true") {
            for (j = 1; j <= 95; j++) printf "-"
            printf "\nShown: %d files\n", count
        }
        exit 0
    }

    # ─── Overview (always printed unless CSV) ────────────────────────────
    if (csv_mode != "true") {
        print "Diff Log Summary"
        for (j = 1; j <= 40; j++) printf "="
        print ""
        printf "  Records:       %d\n", total_records
        printf "  Unique files:  %d\n", unique_files
        printf "  Basenames:     %d\n", n_basenames
        printf "  Status:        \033[1;32m%d PASS\033[0m", n_pass
        if (n_fail > 0) printf "  \033[1;31m%d FAIL\033[0m", n_fail
        else printf "  %d FAIL", n_fail
        if (n_mixed > 0) printf "  \033[1;33m%d MIXED\033[0m", n_mixed
        else printf "  %d MIXED", n_mixed
        print ""
        # Method distribution
        if (length(method_count) > 0) {
            printf "  Methods:      "
            sep = ""
            for (m in method_count) {
                printf "%s %s=%d", sep, m, method_count[m]
                sep = ","
            }
            print ""
        }
        for (j = 1; j <= 40; j++) printf "="
        print ""
    }

    # Determine which basenames to show
    # Default: FAIL + MIXED only (unless --all or --status specified)
    show_pass  = 0
    show_fail  = 1
    show_mixed = 1
    if (show_all == "true") {
        show_pass = 1; show_fail = 1; show_mixed = 1
    }
    if (filter_status == "PASS")  { show_pass = 1; show_fail = 0; show_mixed = 0 }
    if (filter_status == "FAIL")  { show_pass = 0; show_fail = 1; show_mixed = 0 }
    if (filter_status == "MIXED") { show_pass = 0; show_fail = 0; show_mixed = 1 }

    # Sort basenames
    # Simple insertion sort on bn_order by chosen column
    for (a = 2; a <= n_basenames; a++) {
        key = bn_order[a]
        b = a - 1
        if (sort_col == "status") {
            while (b > 0 && bn_status[bn_order[b]] > bn_status[key]) {
                bn_order[b + 1] = bn_order[b]; b--
            }
        } else if (sort_col == "pass") {
            while (b > 0 && bn_pass[bn_order[b]] + 0 < bn_pass[key] + 0) {
                bn_order[b + 1] = bn_order[b]; b--
            }
        } else if (sort_col == "fail") {
            while (b > 0 && bn_fail[bn_order[b]] + 0 < bn_fail[key] + 0) {
                bn_order[b + 1] = bn_order[b]; b--
            }
        } else if (sort_col == "total") {
            while (b > 0 && bn_total[bn_order[b]] + 0 < bn_total[key] + 0) {
                bn_order[b + 1] = bn_order[b]; b--
            }
        } else {
            # name sort (default)
            while (b > 0 && bn_order[b] > key) {
                bn_order[b + 1] = bn_order[b]; b--
            }
        }
        bn_order[b + 1] = key
    }

    # Count how many will be shown
    show_count = 0
    for (i = 1; i <= n_basenames; i++) {
        bn = bn_order[i]
        s = bn_status[bn]
        if (s == "PASS"  && show_pass)  show_count++
        if (s == "FAIL"  && show_fail)  show_count++
        if (s == "MIXED" && show_mixed) show_count++
    }

    if (show_count == 0) {
        if (filter_status != "") {
            printf "No basenames with status %s.\n", filter_status
        }
        exit 0
    }

    # Print table
    if (csv_mode == "true") {
        print "basename,status,pass,fail,total,method,max_err,rmse"
        for (i = 1; i <= n_basenames; i++) {
            bn = bn_order[i]
            s = bn_status[bn]
            if (s == "PASS"  && !show_pass)  continue
            if (s == "FAIL"  && !show_fail)  continue
            if (s == "MIXED" && !show_mixed) continue
            m = (bn in bn_methods_list ? bn_methods_list[bn] : "-")
            e = (bn in bn_max_err ? bn_max_err[bn] : "-")
            r = (bn in bn_max_rmse ? bn_max_rmse[bn] : "-")
            printf "%s,%s,%d,%d,%d,\"%s\",%s,%s\n", bn, s, bn_pass[bn]+0, bn_fail[bn]+0, bn_total[bn], m, e, r
        }
    } else {
        printf "\n%-40s %6s %5s %5s %5s  %-12s %-10s %s\n", "BASENAME", "STATUS", "PASS", "FAIL", "TOTAL", "METHOD", "MAX_ERR", "RMSE"
        for (j = 1; j <= 100; j++) printf "-"
        print ""
        for (i = 1; i <= n_basenames; i++) {
            bn = bn_order[i]
            s = bn_status[bn]
            if (s == "PASS"  && !show_pass)  continue
            if (s == "FAIL"  && !show_fail)  continue
            if (s == "MIXED" && !show_mixed) continue

            m = (bn in bn_methods_list ? bn_methods_list[bn] : "-")
            e = (bn in bn_max_err ? sprintf("%.4g", bn_max_err[bn]) : "-")
            r = (bn in bn_max_rmse ? sprintf("%.4g", bn_max_rmse[bn]) : "-")

            if (s == "FAIL") {
                printf "%-40s \033[1;31m%6s\033[0m %5d %5d %5d  %-12s %-10s %s\n", bn, s, bn_pass[bn]+0, bn_fail[bn]+0, bn_total[bn], m, e, r
            } else if (s == "MIXED") {
                printf "%-40s \033[1;33m%6s\033[0m %5d %5d %5d  %-12s %-10s %s\n", bn, s, bn_pass[bn]+0, bn_fail[bn]+0, bn_total[bn], m, e, r
            } else {
                printf "%-40s \033[1;32m%6s\033[0m %5d %5d %5d  %-12s %-10s %s\n", bn, s, bn_pass[bn]+0, bn_fail[bn]+0, bn_total[bn], m, e, r
            }
        }
        for (j = 1; j <= 100; j++) printf "-"
        printf "\nShown: %d basenames\n", show_count
    }
}
' "${log_files[@]}"
