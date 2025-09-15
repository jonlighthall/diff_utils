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
#   - Optional: tldiff, uband_diff (for advanced numerical comparisons)
#
# Author: Refactored from process_in_files.sh
# Version: 1.0

# Ensure this library is only sourced once
if [[ -n "${FILE_UTILS_LIB_LOADED:-}" ]]; then
    return 0
fi
FILE_UTILS_LIB_LOADED=1

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
# Usage: diff_files <test_file> <reference_file> [threshold1] [threshold2]
# Arguments:
#   test_file  - Path to test/output file
#   ref_file   - Path to reference file
#   threshold1 - Significant error threshold for tldiff/uband_diff (optional)
#   threshold2 - Critical error threshold for uband_diff (optional)
# Returns: 0 if files match (within thresholds), 1 if they differ
# Description:
#   This function performs a cascading comparison:
#   1. Standard diff with color output
#   2. If diff fails, tries tldiff (if available) with threshold1
#   3. If tldiff fails, tries uband_diff (if available) with threshold1 and threshold2
#   4. Reports results with colored output
diff_files() {
    local test="$1"  # test file
    local ref="$2"   # reference file
    local opt1="${3:-}" # significant error threshold for tldiff, uband_diff
    local opt2="${4:-}" # critical error threshold for uband_diff
    
    # Configuration
    local SHOW_LINES=20
    local term_width=$(tput cols 2>/dev/null || echo 80)
    local line_len=$((term_width * 5 / 10))
    
    # Validation
    if [[ $# -lt 2 ]]; then
        echo "Error: diff_files requires at least 2 arguments" >&2
        echo "Usage: diff_files <test_file> <reference_file> [threshold1] [threshold2]" >&2
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
    
    # Temporarily disable exit on error for diff commands
    set +e
    
    # Step 1: Try standard diff
    local tmpfile_diff=$(mktemp)
    diff --color=always --suppress-common-lines -yiEZbwBs "$test" "$ref" > "$tmpfile_diff"
    local RETVAL=$?
    echo "diff done with return code $RETVAL"
    
    if [ $RETVAL -eq 0 ]; then
        # Files are identical
        headtail_truncate "$tmpfile_diff" "$SHOW_LINES"
        rm "$tmpfile_diff"
        echo -e "\e[32mdiff OK\e[0m"
        printf '%*s\n' "$line_len" '' | tr ' ' '-'
        set -e
        return 0
    else
        # Files differ, try advanced tools
        echo "Difference found between $test and $ref"
        headtail_truncate "$tmpfile_diff" "$SHOW_LINES"
        rm "$tmpfile_diff"
        echo -e "\e[31mdiff FAILED\e[0m"
        printf '%*s\n' "$line_len" '' | tr ' ' '-'
        
        # Step 2: Try tldiff if available
        echo "Trying tldiff..."
        if command -v tldiff >/dev/null 2>&1; then
            local tmpfile_tldiff=$(mktemp)
            tldiff "$test" "$ref" $opt1 > "$tmpfile_tldiff" 2>&1
            local tldiff_status=$?
            headtail_truncate "$tmpfile_tldiff" "$SHOW_LINES"
            rm "$tmpfile_tldiff"
            
            if [[ $tldiff_status -eq 0 ]]; then
                echo -e "\e[32mtldiff OK\e[0m"
                printf '%*s\n' "$line_len" '' | tr ' ' '-'
                set -e
                return 0
            else
                echo -e "\e[31mtldiff FAILED\e[0m"
                echo "Trying uband_diff..."
                printf '%*s\n' "$line_len" '' | tr ' ' '-'
                
                # Step 3: Try uband_diff if available
                if command -v uband_diff >/dev/null 2>&1; then
                    local tmpfile_uband=$(mktemp)
                    uband_diff "$test" "$ref" $opt1 $opt2 > "$tmpfile_uband" 2>&1
                    local uband_status=$?
                    headtail_truncate "$tmpfile_uband" "$SHOW_LINES"
                    rm "$tmpfile_uband"
                    
                    if [[ $uband_status -eq 0 ]]; then
                        echo -e "\e[32muband_diff OK\e[0m"
                        printf '%*s\n' "$line_len" '' | tr ' ' '-'
                        set -e
                        return 0
                    else
                        echo -e "\e[31muband_diff FAILED\e[0m"
                        printf '%*s\n' "$line_len" '' | tr ' ' '-'
                        set -e
                        return 1
                    fi
                else
                    echo "Error: uband_diff not found and files differ." >&2
                    set -e
                    return 1
                fi
            fi
        else
            echo "Error: tldiff not found and files differ." >&2
            set -e
            return 1
        fi
    fi
    
    echo "Files differ."
    set -e
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