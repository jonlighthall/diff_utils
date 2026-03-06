#!/bin/bash
# Shell utility library for file processing operations
# Library: file_utils.lib.sh
#
# This library provides reusable functions for file comparison and display utilities.
#
# Usage:
#   source file_utils.lib.sh
#   headtail_truncate "filename" [lines_to_show]
#   diff_files "test_file" "reference_file" [threshold1] [threshold2]
#
# Functions:
#   headtail_truncate - Display head and tail of a file with truncation indicator
#   diff_files        - Advanced file comparison with multiple diff tools
#
# Dependencies:
#   - Standard POSIX utilities (head, tail, wc, diff)
#   - Optional: tldiff, tl_diff (for advanced numerical comparisons)
#
# Author: Refactored from process_in_files.sh
# Version: 1.0

# Ensure this library is only sourced once
if [[ -n "${FILE_UTILS_LIB_LOADED:-}" ]]; then
    return 0
fi
FILE_UTILS_LIB_LOADED=1

#==============================================================================
# OUTPUT PARSING UTILITIES
#==============================================================================

# Function: strip_ansi
# Purpose: Remove ANSI escape sequences from a string
# Usage: clean_val=$(strip_ansi "$raw_val")
strip_ansi() {
    printf '%s' "$1" | sed 's/\x1b\[[0-9;]*m//g'
}

# Function: parse_diff_output
# Purpose: Parse captured diff_files output to recover DIFF_METHOD and stats
#          when diff_files ran in a subshell (e.g., piped through tee).
# Usage: parse_diff_output <output_file>
# Sets: DIFF_METHOD, DIFF_MAX_ERROR, DIFF_N_SIG, DIFF_N_NZ, DIFF_RMSE
parse_diff_output() {
    local output_file="$1"
    DIFF_METHOD="diff"
    DIFF_MAX_ERROR=""
    DIFF_N_SIG=""
    DIFF_N_NZ=""
    DIFF_RMSE=""
    DIFF_RMSE_MAX=""
    DIFF_M1=""

    [[ ! -f "$output_file" ]] && return

    if grep -q "diff FAILED" "$output_file"; then
        if grep -q "tldiff OK\|tldiff FAILED" "$output_file"; then
            DIFF_METHOD="tldiff"
            # Parse tldiff stats
            local max_err
            max_err=$(grep -m1 '^ maximum error :' "$output_file" | awk '{print $NF}')
            [[ -n "$max_err" ]] && DIFF_MAX_ERROR=$(strip_ansi "$max_err")
            local n_sig
            n_sig=$(grep -m1 'number of errors found' "$output_file" | awk '{print $NF}')
            [[ -n "$n_sig" ]] && DIFF_N_SIG=$(strip_ansi "$n_sig")
        fi
        if grep -q "tl_diff OK\|tl_diff FAILED\|TL_DIFF_STATS" "$output_file"; then
            DIFF_METHOD="tl_diff"
            local stats_line
            stats_line=$(grep '^TL_DIFF_STATS' "$output_file")
            if [[ -n "$stats_line" ]]; then
                DIFF_N_NZ=$(echo "$stats_line" | grep -oP 'n_nz=\K[0-9]+')
                DIFF_N_SIG=$(echo "$stats_line" | grep -oP 'n_sig=\K[0-9]+')
                DIFF_MAX_ERROR=$(echo "$stats_line" | grep -oP 'max_nz=\K[0-9.]+')
                DIFF_RMSE=$(echo "$stats_line" | grep -oP 'rmse=\K[0-9.eE+-]+')
                DIFF_RMSE_MAX=$(echo "$stats_line" | grep -oP 'rmse_max=\K[0-9.eE+-]+')
            fi
        fi
        # Parse TL_METRIC_STATS for Fabre M1
        if grep -q '^TL_METRIC_STATS' "$output_file"; then
            local metric_line
            metric_line=$(grep '^TL_METRIC_STATS' "$output_file")
            if [[ -n "$metric_line" ]]; then
                DIFF_M1=$(echo "$metric_line" | grep -oP 'm1=\K[0-9.eE+-]+')
            fi
        fi
    fi
}

#==============================================================================
# DISPLAY UTILITIES
#==============================================================================

# Function: headtail_truncate
# Purpose: Display head and tail of a file with truncation indicator for large files
# Usage: headtail_truncate <file> [lines_to_show]
# Arguments:
#   file          - Path to file to display
#   lines_to_show - Number of lines to show (default: 10)
# Returns: 0 on success, 1 on error
headtail_truncate() {
    local file="$1"
    local SHOW_LINES="${2:-10}"

    # Validation
    if [[ $# -lt 1 ]]; then
        echo "Error: headtail_truncate requires at least 1 argument" >&2
        echo "Usage: headtail_truncate <file> [lines_to_show]" >&2
        return 1
    fi

    if [[ ! -f "$file" ]]; then
        echo "Error: $file does not exist." >&2
        return 1
    fi

    if [[ ! -r "$file" ]]; then
        echo "Error: $file is not readable." >&2
        return 1
    fi

    # Get total line count
    local lines
    lines=$(wc -l < "$file")

    if [[ $lines -le $SHOW_LINES ]]; then
        # File is small enough to show completely
        cat "$file"
        echo "--- Total: $lines lines ---"
    else
        # File is large, show head and tail with truncation indicator
        local head_lines tail_lines
        local half_lines=$((SHOW_LINES / 2))

        head_lines=$(head -$half_lines "$file")
        tail_lines=$(tail -$half_lines "$file")

        echo "$head_lines"
        echo "... (output truncated) ..."

        # Show unique tail lines (avoid duplicates if file has repeated content)
        comm -13 <(echo "$head_lines" | sort) <(echo "$tail_lines" | sort)

        local hidden_lines=$((lines - SHOW_LINES))
        echo "--- Total: $lines lines, showing $SHOW_LINES lines, $hidden_lines lines hidden ---"
    fi
}

#==============================================================================
# FILE COMPARISON UTILITIES
#==============================================================================

# Function: diff_files
# Purpose: Advanced file comparison with multiple diff tools and fallbacks
# Usage: diff_files <test_file> <reference_file> [threshold1] [threshold2] [diff_level]
# Arguments:
#   test_file  - Path to test/output file
#   ref_file   - Path to reference file
#   threshold1 - Significant error threshold for tldiff/tl_diff (optional)
#   threshold2 - Critical error threshold for tl_diff (optional)
#   diff_level - Force specific diff level: 1=diff only, 2=tldiff max, 3=tl_diff (optional)
# Returns: 0 if files match (within thresholds), 1 if they differ
# Description:
#   This function performs a cascading comparison:
#   Default behavior (no diff_level):
#     1. Standard diff with color output
#     2. If diff fails, tries tldiff (if available) with threshold1
#     3. If tldiff fails, tries tl_diff (if available) with threshold1 and threshold2
#   With diff_level:
#     1: Only run standard diff (no fallback)
#     2: Run up to tldiff (skip tl_diff even if tldiff fails)
#     3: Run all the way to tl_diff (always run all three, report last result)
#   4. Reports results with colored output
diff_files() {
    local test="$1"  # test file
    local ref="$2"   # reference file
    local opt1="${3:-}" # significant error threshold for tldiff, tl_diff
    local opt2="${4:-}" # critical error threshold for tl_diff
    local diff_level="${5:-0}"  # diff level override: 0=auto (default), 1=diff only, 2=max tldiff, 3=force tl_diff

    # Configuration
    local SHOW_LINES=20
    local term_width=$(tput cols 2>/dev/null || echo 80)
    local line_len=$((term_width * 5 / 10))

    # Validation
    if [[ $# -lt 2 ]]; then
        echo "Error: diff_files requires at least 2 arguments" >&2
        echo "Usage: diff_files <test_file> <reference_file> [threshold1] [threshold2] [diff_level]" >&2
        return 1
    fi

    # Validate diff_level
    if [[ -n "$diff_level" && ! "$diff_level" =~ ^[0-3]$ ]]; then
        echo "Error: diff_level must be 0 (auto), 1 (diff only), 2 (max tldiff), or 3 (force tl_diff)" >&2
        return 1
    fi

    # Check test file
    if [[ ! -f "$test" ]]; then
        echo -e "\e[31mError: Test file '$test' does not exist.\e[0m" >&2
        return 1
    fi
    if [[ ! -s "$test" ]]; then
        echo -e "\e[31mError: Test file '$test' is empty.\e[0m" >&2
        return 1
    fi

    # Check reference file
    if [[ ! -f "$ref" ]]; then
        echo -e "\e[31mError: Reference file '$ref' does not exist.\e[0m" >&2
        return 1
    fi
    if [[ ! -s "$ref" ]]; then
        echo -e "\e[31mError: Reference file '$ref' is empty.\e[0m" >&2
        return 1
    fi

    # Display comparison header
    echo
    printf '%*s\n' "$line_len" '' | tr ' ' '-'
    echo "Diffing $test and $ref:"
    if [[ $diff_level -eq 1 ]]; then
        echo "  (Level 1: diff only - no fallback)"
    elif [[ $diff_level -eq 2 ]]; then
        echo "  (Level 2: max tldiff - stop at tldiff)"
    elif [[ $diff_level -eq 3 ]]; then
        echo "  (Level 3: force tl_diff - run all comparisons)"
    fi

    # Check if errexit (set -e) is currently enabled and save the state
    local errexit_was_set=false
    if [[ $- == *e* ]]; then
        errexit_was_set=true
    fi

    # Temporarily disable exit on error for diff commands
    set +e

    # Helper function to restore errexit state
    restore_errexit() {
        if [[ "$errexit_was_set" == true ]]; then
            set -e
        fi
    }

    # Initialize DIFF_METHOD and DIFF_MAX_ERROR to track results
    DIFF_METHOD=""
    DIFF_MAX_ERROR=""    # overall max error (from tldiff or tl_diff)
    DIFF_N_SIG=""        # number of significant differences (from tl_diff)
    DIFF_N_NZ=""         # number of non-zero differences (from tl_diff)
    DIFF_RMSE=""         # full-field RMSE (from tl_diff)
    DIFF_M1=""           # Fabre M1 weighted mean absolute difference (from tl_metric)

    # Step 1: Try standard diff
    local tmpfile_diff=$(mktemp)
    diff --color=always --suppress-common-lines -yiEZbwBs "$test" "$ref" > "$tmpfile_diff"
    local RETVAL=$?
    local diff_passed=false
    echo "diff done with return code $RETVAL"

    if [ $RETVAL -eq 0 ]; then
        # Files are identical
        headtail_truncate "$tmpfile_diff" "$SHOW_LINES"
        rm "$tmpfile_diff"
        echo -e "\e[32mdiff OK\e[0m"
        printf '%*s\n' "$line_len" '' | tr ' ' '-'
        DIFF_METHOD="diff"
        diff_passed=true

        # Level 3: continue to tl_diff even when diff passes
        if [[ $diff_level -ne 3 ]]; then
            restore_errexit
            return 0
        fi
        echo "Continuing to level 3 (force tl_diff) despite diff OK..."
    else
        # Files differ
        echo "Difference found between $test and $ref"
        headtail_truncate "$tmpfile_diff" "$SHOW_LINES"
        rm "$tmpfile_diff"
        echo -e "\e[31mdiff FAILED\e[0m"
        printf '%*s\n' "$line_len" '' | tr ' ' '-'

        # If diff_level is 1, stop here (diff only mode)
        if [[ $diff_level -eq 1 ]]; then
            echo "Stopping at level 1 (diff only)"
            DIFF_METHOD="diff"
            restore_errexit
            return 1
        fi
    fi

    # Step 2: Try tldiff if available (skip for level 3 when diff passed)
    if [[ "$diff_passed" != true ]]; then
        echo "Trying tldiff..."
        if command -v tldiff >/dev/null 2>&1; then
            local tmpfile_tldiff=$(mktemp)
            tldiff "$test" "$ref" $opt1 > "$tmpfile_tldiff" 2>&1
            local tldiff_status=$?
            headtail_truncate "$tmpfile_tldiff" "$SHOW_LINES"

            # Parse tldiff stats from output before removing temp file
            DIFF_MAX_ERROR=$(strip_ansi "$(grep -m1 '^ maximum error :' "$tmpfile_tldiff" | awk '{print $NF}')")
            DIFF_N_SIG=$(strip_ansi "$(grep -m1 '^  *number of errors found' "$tmpfile_tldiff" | awk '{print $NF}')")
            rm "$tmpfile_tldiff"

            if [[ $tldiff_status -eq 0 ]]; then
                echo -e "\e[32mtldiff OK\e[0m"
                printf '%*s\n' "$line_len" '' | tr ' ' '-'
                if [[ $diff_level -eq 3 ]]; then
                    echo "Continuing to level 3 (force tl_diff)..."
                else
                    DIFF_METHOD="tldiff"
                    restore_errexit
                    return 0
                fi
            else
                echo -e "\e[31mtldiff FAILED\e[0m"
                printf '%*s\n' "$line_len" '' | tr ' ' '-'

                if [[ $diff_level -eq 2 ]]; then
                    echo "Stopping at level 2 (max tldiff)"
                    DIFF_METHOD="tldiff"
                    restore_errexit
                    return 1
                fi
            fi
        else
            if [[ $diff_level -ne 3 ]]; then
                echo "Error: tldiff not found and files differ." >&2
                DIFF_METHOD="diff"
                restore_errexit
                return 1
            fi
            echo "tldiff not found, skipping to tl_diff..."
        fi
    fi

    # Step 3: Try tl_diff if available
    if [[ $diff_level -ne 2 ]]; then
        echo "Trying tl_diff..."
        if command -v tl_diff >/dev/null 2>&1; then
            local tmpfile_uband=$(mktemp)
            tl_diff "$test" "$ref" $opt1 $opt2 > "$tmpfile_uband" 2>&1
            local uband_status=$?
            headtail_truncate "$tmpfile_uband" "$SHOW_LINES"

            # Parse TL_DIFF_STATS from output before removing temp file
            local stats_line
            stats_line=$(grep '^TL_DIFF_STATS' "$tmpfile_uband" || true)
            if [[ -n "$stats_line" ]]; then
                DIFF_N_NZ=$(echo "$stats_line" | grep -oP 'n_nz=\K[0-9]+')
                DIFF_N_SIG=$(echo "$stats_line" | grep -oP 'n_sig=\K[0-9]+')
                DIFF_MAX_ERROR=$(echo "$stats_line" | grep -oP 'max_nz=\K[0-9.]+')
                DIFF_RMSE=$(echo "$stats_line" | grep -oP 'rmse=\K[0-9.eE+-]+')
                DIFF_RMSE_MAX=$(echo "$stats_line" | grep -oP 'rmse_max=\K[0-9.eE+-]+')
            fi
            rm "$tmpfile_uband"

            if [[ $uband_status -eq 0 ]]; then
                echo -e "\e[32mtl_diff OK\e[0m"
                printf '%*s\n' "$line_len" '' | tr ' ' '-'
                DIFF_METHOD="tl_diff"
                # Run tl_metric for Fabre M1 if available
                if command -v tl_metric >/dev/null 2>&1; then
                    local tmpfile_metric=$(mktemp)
                    tl_metric "$test" "$ref" > "$tmpfile_metric" 2>&1
                    local metric_stats
                    metric_stats=$(grep '^TL_METRIC_STATS' "$tmpfile_metric" || true)
                    if [[ -n "$metric_stats" ]]; then
                        DIFF_M1=$(echo "$metric_stats" | grep -oP 'm1=\K[0-9.eE+-]+')
                    fi
                    rm "$tmpfile_metric"
                fi
                restore_errexit
                return 0
            else
                echo -e "\e[31mtl_diff FAILED\e[0m"
                printf '%*s\n' "$line_len" '' | tr ' ' '-'
                DIFF_METHOD="tl_diff"
                # Run tl_metric for Fabre M1 even on failure if available
                if command -v tl_metric >/dev/null 2>&1; then
                    local tmpfile_metric=$(mktemp)
                    tl_metric "$test" "$ref" > "$tmpfile_metric" 2>&1
                    local metric_stats
                    metric_stats=$(grep '^TL_METRIC_STATS' "$tmpfile_metric" || true)
                    if [[ -n "$metric_stats" ]]; then
                        DIFF_M1=$(echo "$metric_stats" | grep -oP 'm1=\K[0-9.eE+-]+')
                    fi
                    rm "$tmpfile_metric"
                fi
                restore_errexit
                return 1
            fi
        else
            echo "Error: tl_diff not found." >&2
            restore_errexit
            return 1
        fi
    fi

    echo "Files differ."
    DIFF_METHOD="diff"
    restore_errexit
    return 1
}

#==============================================================================
# LIBRARY INFORMATION
#==============================================================================

# Function: file_utils_version
# Purpose: Display library version and available functions
file_utils_version() {
    echo "file_utils.lib.sh - Version 1.0"
    echo "Available functions:"
    echo "  headtail_truncate - Display file with truncation"
    echo "  diff_files        - Advanced file comparison"
    echo "  file_utils_version - Show this information"
}

# If script is run directly (not sourced), show usage
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    echo -e "\e[33mThis is a library file meant to be sourced.\e[0m"
    echo "Usage: source ${BASH_SOURCE[0]}"
    echo
    file_utils_version
    exit 1
fi

# Indicate that the library has been loaded
echo "$(basename ${BASH_SOURCE[0]}) loaded"