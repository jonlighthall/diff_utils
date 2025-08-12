#!/bin/bash
# Usage: ./process_in_files.sh <mode> <input_directory> [target_directory]


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


find "$indir" -maxdepth 1 -type f -name '*.in' | while read -r infile; do
  # Check for required strings (case-insensitive)
  if grep -qi '^tl' "$infile" || grep -qi '^rtl' "$infile" || grep -Eiq '^hrfa|^hfra|^hari' "$infile"; then
    echo "Processing $infile..."

      ./nspe.exe "$infile"
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
            echo "Diffing $src and $dest:"
            if diff "$src" "$dest"; then
              # Print OK in green
              echo -e "\e[32mOK\e[0m"
            else
              echo "Files differ. Exiting."
              exit 1
            fi
          else
            echo "Unknown mode: $mode"
            exit 1
          fi
          break
        fi
      done


  else
    echo "Skipping $infile (does not contain required strings)"
  fi
done
