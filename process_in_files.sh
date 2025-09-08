#!/bin/bash
# Usage: ./process_in_files.sh <mode> [directory]
# =============================
# EXPLANATION SECTION
#
# This script processes .in files in a directory using nspe.exe.
#
# Input options (first argument):
#   make - Runs nspe.exe and renames output files to standard names (.tl, .rtl, .ftl)
#   test - Runs nspe.exe, logs output, and compares results to reference files
#   diff - Compares existing output files to reference files (does not run nspe.exe)
#
# PROMPT: To process files, run:
#   ./process_in_files.sh <mode> [directory]
# Where <mode> is 'make', 'test', or 'diff'. Directory defaults to 'std'.
# =============================

headtail_truncate() {
    local file="$1"
    local SHOW_LINES="${2:-10}"

    if [[ ! -f "$file" ]]; then
        echo "Error: $file does not exist."
        return 1
    fi

    local lines
    lines=$(wc -l < "$file")
    if [[ $lines -le $SHOW_LINES ]]; then
        cat "$file"
    else
        local head_lines tail_lines
        head_lines=$(head -$((SHOW_LINES/2)) "$file")
        tail_lines=$(tail -$((SHOW_LINES/2)) "$file")
        echo "$head_lines"
        echo "... (output truncated) ..."
        comm -13 <(echo "$head_lines" | sort) <(echo "$tail_lines" | sort)
    fi
}

diff_files() {
    local src="$1"
    local dest="$2"
    local opt1="${3:-}"
    local opt2="${4:-}"

    local SHOW_LINES=20
    term_width=$(tput cols 2>/dev/null || echo 80)
    line_len=$((term_width * 9 / 10))

    if [[ ! -f "$src" ]]; then
        echo -e "\e[31mError: Source file '$src' does not exist. Exiting.\e[0m"
        exit 1
    fi
    if [[ ! -f "$dest" ]]; then
        echo -e "\e[31mError: Destination file '$dest' does not exist. Exiting.\e[0m"
        exit 1
    fi
    if [[ ! -s "$src" ]]; then
        echo -e "\e[31mError: Source file '$src' is empty. Exiting.\e[0m"
        exit 1
    fi
    if [[ ! -s "$dest" ]]; then
        echo -e "\e[31mError: Destination file '$dest' is empty. Exiting.\e[0m"
        exit 1
    fi

    printf '%*s\n' "$line_len" '' | tr ' ' '-'
    echo "Diffing $src and $dest:"
    set +e
    tmpfile_diff=$(mktemp)
    diff --color=always --suppress-common-lines -yiEZbwBs "$src" "$dest" > "$tmpfile_diff"
    RETVAL=$?
    echo "diff done with return code $RETVAL"
    diff_lines=$(wc -l < "$tmpfile_diff")
    if [ $RETVAL -eq 0 ]; then
        headtail_truncate "$tmpfile_diff" "$SHOW_LINES"
        rm "$tmpfile_diff"
        echo -e "\e[32mOK\e[0m"
        printf '%*s\n' "$line_len" '' | tr ' ' '-'
        return 0
    else
        echo "Difference found between $src and $dest"

        headtail_truncate "$tmpfile_diff" "$SHOW_LINES"
        rm "$tmpfile_diff"
        echo -e "\e[31mdiff FAILED\e[0m"
        printf '%*s\n' "$line_len" '' | tr ' ' '-'
        echo "Trying tldiff..."
        if command -v tldiff >/dev/null 2>&1; then

            tmpfile_tldiff=$(mktemp)
            tldiff "$src" "$dest" $opt1 > "$tmpfile_tldiff" 2>&1
            tldiff_status=$?
            headtail_truncate "$tmpfile_tldiff" "$SHOW_LINES"
            rm "$tmpfile_tldiff"
            if [[ $tldiff_status -eq 0 ]]; then
                echo -e "\e[32mtldiff OK\e[0m"
                printf '%*s\n' "$line_len" '' | tr ' ' '-'
                return 0
            else
                echo -e "\e[31mtldiff FAILED\e[0m"
                echo "Trying uband_diff..."
                printf '%*s\n' "$line_len" '' | tr ' ' '-'
                if command -v uband_diff >/dev/null 2>&1; then

                    tmpfile_uband=$(mktemp)
                    uband_diff "$src" "$dest" $opt1 $opt2 > "$tmpfile_uband" 2>&1
                    uband_status=$?
                    headtail_truncate "$tmpfile_uband" "$SHOW_LINES"
                    rm "$tmpfile_uband"
                    if [[ $uband_status -eq 0 ]]; then
                        echo -e "\e[32muband_diff OK\e[0m"
                        printf '%*s\n' "$line_len" '' | tr ' ' '-'
                        return 0
                    else
                        echo -e "\e[31muband_diff FAILED\e[0m"
                        printf '%*s\n' "$line_len" '' | tr ' ' '-'
                        return 1
                    fi
                else
                    echo "Error: uband_diff not found and files differ."
                    return 1
                fi
            fi
        else
            echo "Error: tldiff not found and files differ."
            return 1
        fi
        echo "Files differ."
        return 1
    fi
}

set -e
mode="$1"
# =============================
# MODE EXPLANATION
#
# What do you want to do with respect to the reference output files?
#
# The script operates in three modes: 'make', 'test', and 'diff'.
#
# 'make':
#   - Runs nspe.exe on each .in file
#   - Renames output files (_01.asc, _02.asc, _03.asc) to standard names (.tl, .rtl, .ftl)
#   - Cleans up extra files
#   - Use when you want to generate and prepare outputs for reference
#
# 'test':
#   - Runs nspe.exe on each .in file
#   - Logs output to a .log file
#   - Compares output files to reference files using diff_files
#   - Reports PASS/FAIL for each test
#   - Use to verify correctness of outputs by running fresh simulations
#
# 'diff':
#   - Does NOT run nspe.exe
#   - Compares existing output files (_01.asc, _02.asc, _03.asc) to reference files (.tl, .rtl, .ftl)
#   - Shows differences using diff_files
#   - Use to manually inspect differences between existing outputs and references
# =============================
directory="${2:-std}"

# Validate mode
if [[ -z "$mode" ]]; then
    echo "Usage: $0 <mode> [directory]"
    echo "mode: make, test, or diff"
    echo "directory: defaults to 'std' if not specified"
    exit 1
fi

if [[ ! -d "$directory" ]]; then
    echo "Error: Directory '$directory' does not exist. Exiting."
    exit 1
fi

PROG=./nspe.exe

# Create array of files to process, sorted by size
mapfile -t infiles < <(find "$directory" -maxdepth 1 -type f -name '*.in' -exec ls -lSr {} + | awk '{print $9}')

if [[ ${#infiles[@]} -eq 0 ]]; then
    echo
    echo -e "\e[31mNo input files (*.in) found in directory: $directory\e[0m"

    exit 1
fi
pass_files=()
fail_files=()
for infile in "${infiles[@]}"; do
    # Check for required strings (case-insensitive)
    if grep -qi '^tl' "$infile" || grep -qi '^rtl' "$infile" || grep -Eiq '^hrfa|^hfra|^hari' "$infile"; then
            echo -e "\n=============================="
            echo "Processing input file: $infile"
            echo "Mode: $mode"
            echo "=============================="
        LOG_FILE="$directory/$(basename "$infile" .in).log"

        # Run nspe.exe for 'make' and 'test' modes, but not for 'diff' mode
        if [[ "$mode" == "make" || "$mode" == "test" ]]; then
            if [[ "$mode" == "test" ]]; then
                echo "[TEST MODE] Running: $PROG $infile"
                { time timeout 300s "$PROG" "$infile"; } >> "$LOG_FILE" 2>&1
            else
                echo "[MAKE MODE] Running: $PROG $infile"
                timeout 300s "$PROG" "$infile"
                if [[ $? -ne 0 ]]; then
                    echo "Error: nspe.exe failed for $infile. Continuing to next file."
                    continue
                fi
            fi
        else
            echo "[DIFF MODE] Skipping nspe.exe execution, comparing existing files..."
        fi
        # Rename only one file per priority: 03, 02, 01
        for suffix in 03 02 01; do
            src="$directory/$(basename "$infile" .in)_${suffix}.asc"
            case $suffix in
                01) dest="$directory/$(basename "$infile" .in).tl" ;;
                02) dest="$directory/$(basename "$infile" .in).rtl" ;;
                03) dest="$directory/$(basename "$infile" .in).ftl" ;;
            esac
            if [[ -f "$src" && -s "$src" ]]; then
                if [[ "$mode" == "make" ]]; then
                    # MAKE mode: Only rename files, no comparison
                    mv "$src" "$dest"
                    echo "Renamed $src to $dest"
                elif [[ "$mode" == "test" || "$mode" == "diff" ]]; then
                    # TEST and DIFF modes: Compare files
                    echo "Comparing $src to $dest for $infile..."
                    if [[ "$mode" == "test" ]]; then
                        # TEST mode: Log comparison results
                        if diff_files "$src" "$dest" >> "$LOG_FILE" 2>&1; then
                            echo -e "\e[32m[[PASS]]\e[0m $infile" # Print PASS to terminal
                            echo -e "\e[32m[[PASS]]\e[0m" >> "$LOG_FILE"
                            pass_files+=("$infile")
                        else
                            echo -e "\e[31m[[FAIL]]\e[0m $infile" # Print FAIL to terminal
                            echo -e "\e[31m[[FAIL]]\e[0m" >> "$LOG_FILE"
                            fail_files+=("$infile")
                        fi
                    else
                        # DIFF mode: Show comparison results to terminal only
                        if diff_files "$src" "$dest"; then
                            echo -e "\e[32m[[PASS]]\e[0m $infile" # Print PASS to terminal
                            pass_files+=("$infile")
                        else
                            echo -e "\e[31m[[FAIL]]\e[0m $infile" # Print FAIL to terminal
                            fail_files+=("$infile")
                        fi
                    fi
                else
                    echo "Unknown mode: $mode"
                    exit 1
                fi
                # After renaming/diffing, delete files with same base name except .in and selected destination
                basename_noext="$(basename "$infile" .in)"
                for f in "$directory/$basename_noext"*; do
                    # Skip .in, selected destination, and src file
                    if [[ "$f" == "$infile" || "$f" == "$dest" || "$f" == "$src" ]]; then
                        continue
                    fi
                    # Skip log file in test mode
                    if [[ "$f" == "$LOG_FILE" && "$mode" == "test" ]]; then
                        continue
                    fi
                    # Skip if src file contains "case7" or "case6"
                    if [[ "$f" == "$LOG_FILE" ]] && [[ "$src" == *case7* || "$src" == *case6* ]]; then
                        continue
                    fi
                    # Only delete files
                    if [[ -f "$f" ]]; then
                        rm "$f"
                        echo "   Deleted $f"
                    fi
                done
                break
            fi
        done
    else
        echo -e "\e[33mSkipping $infile (does not contain required strings)\e[0m"
    fi
done

# Print summary if in test or diff mode
if [[ "$mode" == "test" || "$mode" == "diff" ]]; then
    echo -e "\n=============================="
    if [[ "$mode" == "test" ]]; then
        echo "Test Summary:"
    else
        echo "Diff Summary:"
    fi
    echo "=============================="
    echo "Passed files: ${#pass_files[@]}"
    for f in "${pass_files[@]}"; do
        echo -e "  \e[32mPASS\e[0m $f"
    done
    echo "Failed files: ${#fail_files[@]}"
    for f in "${fail_files[@]}"; do
        echo -e "  \e[31mFAIL\e[0m $f"
    done
    if [[ ${#fail_files[@]} -eq 0 ]]; then
        echo -e "\nAll files passed!"
    fi
    echo "=============================="
fi
