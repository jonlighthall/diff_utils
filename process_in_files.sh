#!/bin/bash
# Usage: ./process_in_files.sh <mode> [directory]

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

    printf '%*s\n' "$line_len" '' | tr ' ' '-'
    echo "Diffing $src and $dest:"
    set +e
    tmpfile_diff=$(mktemp)
    diff --color=always --suppress-common-lines -yiEZbwB "$src" "$dest" > "$tmpfile_diff"
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

# Process each file
for infile in "${infiles[@]}"; do
    # Check for required strings (case-insensitive)
    if grep -qi '^tl' "$infile" || grep -qi '^rtl' "$infile" || grep -Eiq '^hrfa|^hfra|^hari' "$infile"; then
        echo "Processing $infile..."
        LOG_FILE="$directory/$(basename "$infile" .in).log"
        if [[ "$mode" == "test" ]]; then

            { time timeout 300s "$PROG" "$infile"; } >> "$LOG_FILE" 2>&1
        else
            # Run nspe.exe in isolation to avoid interfering with the loop
            timeout 300s "$PROG" "$infile"
            if [[ $? -ne 0 ]]; then
                echo "Error: nspe.exe failed for $infile. Continuing to next file."
                continue
            fi
        fi
        # Rename only one file per priority: 03, 02, 01
        for suffix in 03 02 01; do
            src="$directory/$(basename "$infile" .in)_${suffix}.asc"
            case $suffix in
                01) dest="$directory/$(basename "$infile" .in).tl" ;;
                02) dest="$directory/$(basename "$infile" .in).rtl" ;;
                03) dest="$directory/$(basename "$infile" .in).ftl" ;;
            esac
            if [[ -f "$src" ]]; then
                if [[ "$mode" == "make" ]]; then
                    mv "$src" "$dest"
                    echo "Renamed $src to $dest"
                elif [[ "$mode" == "diff" ]]; then
                    # Call the function with src and dest, and optionally extra
                    diff_files "$src" "$dest"
                    elif [[  "$mode" == "test" ]]; then
                        if diff_files "$src" "$dest" >> "$LOG_FILE" 2>&1; then
                            echo -e "\e[32m[[PASS]]\e[0m" >> "$LOG_FILE"
                        else
                            echo -e "\e[31m[[FAIL]]\e[0m" >> "$LOG_FILE"
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
