#!/bin/bash
# Usage: ./process_in_files.sh <mode> <input_directory> [target_directory]

diff_files() {
    local src="$1"
    local dest="$2"
    local opt1="${3:-}"
    local opt2="${4:-}"

    echo "Diffing $src and $dest:"
    if diff "$src" "$dest"; then
        echo -e "\e[32mOK\e[0m"
        return 0
    else
        echo "Difference found between $src and $dest"
        if command -v tldiff >/dev/null 2>&1; then
            echo "Trying tldiff..."
            if tldiff "$src" "$dest" $opt1 $opt2; then
                echo -e "\e[32mtldiff OK\e[0m"
                return 0
            else
                if command -v uband_diff >/dev/null 2>&1; then
                    echo "Trying uband_diff..."
                    if uband_diff "$src" "$dest" $opt1 $opt2; then
                        echo -e "\e[32muband_diff OK\e[0m"
                        return 0
                    else
                        echo "Error: Files differ after tldiff and uband_diff. Exiting."
                        exit 1
                    fi
                else
                    echo "Error: uband_diff not found and files differ. Exiting."
                    exit 1
                fi
            fi
        else
            echo "Error: tldiff not found and files differ. Exiting."
            exit 1
        fi
        echo "Files differ. Exiting."
        exit 1
    fi
}

set -e
mode="$1"
indir="$2"
targetdir="$3"

if [[ -z "$mode" || -z "$indir" ]]; then
    echo "Usage: $0 <mode> <input_directory> [target_directory]"
    echo "mode: make, test, or diff"
    exit 1
fi
# Default targetdir to indir if not specified
if [[ -z "$targetdir" ]]; then
    targetdir="$indir"
fi

find "$indir" -maxdepth 1 -type f -name '*.in' -exec ls -lSr {} + | awk '{print $9}' | while read -r infile; do
    # Check for required strings (case-insensitive)
    if grep -qi '^tl' "$infile" || grep -qi '^rtl' "$infile" || grep -Eiq '^hrfa|^hfra|^hari' "$infile"; then
        echo "Processing $infile..."
        if ! ./nspe.exe "$infile"; then
            echo "Error: nspe.exe failed for $infile. Exiting."
            exit 1
        fi
        # Rename only one file per priority: 03, 02, 01
        for suffix in 03 02 01; do
            src="$targetdir/$(basename "$infile" .in)_${suffix}.asc"
            case $suffix in
                01) dest="$targetdir/$(basename "$infile" .in).tl" ;;
                02) dest="$targetdir/$(basename "$infile" .in).rtl" ;;
                03) dest="$targetdir/$(basename "$infile" .in).ftl" ;;
            esac
            if [[ -f "$src" ]]; then
                if [[ "$mode" == "make" ]]; then
                    mv "$src" "$dest"
                    echo "Renamed $src to $dest"
                elif [[ "$mode" == "test" || "$mode" == "diff" ]]; then

                    # Call the function with src and dest, and optionally extra args
                    diff_files "$src" "$dest"
                else
                    echo "Unknown mode: $mode"
                    exit 1
                fi
                # After renaming/diffing, delete files with same base name except .in and selected destination
                basename_noext="$(basename "$infile" .in)"
                for f in "$targetdir/$basename_noext"*; do
                    # Skip .in, selected destination, and src file
                    if [[ "$f" == "$infile" || "$f" == "$dest" || "$f" == "$src" ]]; then
                        continue
                    fi
                    # Only delete files
                    if [[ -f "$f" ]]; then
                        rm "$f"
                        echo "Deleted $f"
                    fi
                done
                break
            fi
        done
    else
        echo "Skipping $infile (does not contain required strings)"
    fi
done
