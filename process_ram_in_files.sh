#!/bin/bash
# Usage: ./process_ram_in_files.sh <mode> [directory]
# Sep 2025 JCL
# =============================
# EXPLANATION SECTION
#
# This script processes .in files in a directory using bin/ram1.5.exe.
#
# Input options (first argument):
#   make - Runs ram1.5.exe only (no file operations)
#   copy - Copies existing output files (basename.line) to reference names (.tl)
#   test - Runs ram1.5.exe, logs output, and compares results to reference files
#   diff - Compares existing output files to reference files (does not run ram1.5.exe)
#
# PROMPT: To process files, run:
#   ./process_ram_in_files.sh <mode> [directory]
# Where <mode> is 'make', 'copy', 'test', or 'diff'. Directory defaults to 'std'.
#
# Dependencies:
#   - lib_diff_utils.sh (must be in same directory or PATH)
#   - bin/ram1.5.exe
#   - Optional: tldiff, uband_diff for advanced comparisons
#
# =============================

# Source the utility library
SCRIPT_DIR="$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"
LIBRARY_FILE="${SCRIPT_DIR}/lib_diff_utils.sh"

if [[ -f "$LIBRARY_FILE" ]]; then
    source "$LIBRARY_FILE"
else
    echo -e "\e[31mError: Required library file '$LIBRARY_FILE' not found.\e[0m" >&2
    echo "Please ensure lib_diff_utils.sh is in the same directory as this script." >&2
    exit 1
fi

set -e
mode="$1"
# =============================
# MODE EXPLANATION
#
# What do you want to do with respect to the output files?
#
# The script operates in four modes: 'make', 'copy', 'test', and 'diff'.
#
# 'make':
#   - Runs ram1.5.exe on each .in file (generates output files)
#   - Does not perform any file operations (move, copy, diff)
#   - Use when you just want to generate output files
#
# 'copy':
#   - Does NOT run ram1.5.exe
#   - Copies existing output files (.line) to reference names (.tl)
#   - Cleans up extra files
#   - Use when you want to prepare existing outputs for reference
#
# 'test':
#   - Runs ram1.5.exe on each .in file
#   - Logs output to a .log file
#   - Compares output files to reference files using diff_files
#   - Reports PASS/FAIL for each test
#   - Use to verify correctness of outputs by running fresh simulations
#
# 'diff':
#   - Does NOT run ram1.5.exe
#   - Compares existing output files (.line) to reference files (.tl)
#   - Shows differences using diff_files
#   - Use to manually inspect differences between existing outputs and references
# =============================
directory="${2:-std}"

# Remove trailing slash from directory to avoid double slashes in file paths
directory="${directory%/}"

# Validate mode
if [[ -z "$mode" ]]; then
    echo -e "\e[31mError: No mode specified.\e[0m"
    echo "Usage: $0 <mode> [directory]"
    echo "mode: make, copy, test, or diff"
    echo "directory: defaults to 'std' if not specified"
    exit 1
fi

# Validate that mode is one of the accepted values
if [[ "$mode" != "make" && "$mode" != "copy" && "$mode" != "test" && "$mode" != "diff" ]]; then
    echo -e "\e[31mError: Invalid mode '$mode'.\e[0m"
    echo "Valid modes are: make, copy, test, or diff"
    echo ""
    echo "Mode descriptions:"
    echo "  make - Run RAM only (no file operations)"
    echo "  copy - Copy existing output files (.line) to reference files (.tl)"
    echo "  test - Time and run RAM and compare outputs to reference files"
    echo "  diff - Compare existing output files to reference files (no RAM execution)"
    exit 1
fi

if [[ ! -d "$directory" ]]; then
    echo "Error: Directory '$directory' does not exist. Exiting."
    exit 1
fi

# Set the program to bin/ram1.5.exe
PROG="bin/ram1.5.exe"
PROG_OUTPUT_COLOR="\x1B[38;5;71m" # Light green color for PROG output

# Detect the appropriate ram program (skip for diff and copy modes)
if [[ "$mode" != "diff" && "$mode" != "copy" ]]; then
    echo "Selected program: $PROG"
    
    # Check if the program exists, and build it if not
    if [[ ! -f "$PROG" ]]; then
        echo "Program $PROG not found. Attempting to build it..."
        make
    fi
    
    # Verify the program is now executable
    if [[ ! -x "$PROG" ]]; then
        echo -e "\e[31mError: $PROG exists but is not executable.\e[0m"
        echo "Making it executable..."
        chmod +x "$PROG"
    fi
else
    echo "Copy/Diff mode: Skipping $PROG program detection and execution"
fi

# Create array of files to process, sorted by size
mapfile -t infiles < <(find "$directory" -maxdepth 1 -type f -name '*.in' -exec ls -lSr {} + | awk '{print $9}')

if [[ ${#infiles[@]} -eq 0 ]]; then
    echo
    echo -e "\e[31mNo input files (*.in) found in directory: $directory\e[0m"
    
    exit 1
fi
pass_files=()
fail_files=()
missing_reference_files=()
missing_output_files=()
empty_files=()
processed_files=()
skipped_files=()

# Print mode header once at the beginning
term_width=$(tput cols 2>/dev/null || echo 80)
line_len=$((term_width * 5 / 10))

printf '%*s\n' "$line_len" '' | tr '  ' '='
echo "Processing directory: $directory"
echo "Mode: $mode"
if [[ "$mode" == "make" ]]; then
    echo "Will run ${PROG} only (no file operations)"
    elif [[ "$mode" == "copy" ]]; then
    echo "Will copy existing output files (.line) to reference files (.tl)"
    elif [[ "$mode" == "test" ]]; then
    echo "Will run ${PROG} and compare outputs to reference files"
    elif [[ "$mode" == "diff" ]]; then
    echo "Will compare existing output files to reference files (no ${PROG} execution)"
fi
printf '%*s\n' "$line_len" '' | tr '  ' '='

for infile in "${infiles[@]}"; do
    echo "Processing: $infile"
    LOG_FILE="$directory/$(basename "$infile" .in).log"
    basename_noext="$(basename "$infile" .in)"
    parent_dir="$(dirname "$directory")"
    
    # Run ram1.5.exe for 'make' and 'test' modes, but not for 'copy' or 'diff' mode
    if [[ "$mode" == "make" || "$mode" == "test" ]]; then
        # Copy infile to ram.in in parent directory
        cp "$infile" "$parent_dir/ram.in"
        
        # Check for existing output files and warn user
        existing_files=()
        potential_output="$parent_dir/tl.line"
        if [[ -f "$potential_output" ]]; then
            existing_files+=("$potential_output")
        fi
        
        # Also check for other common output files
        for ext in .log; do
            potential_output="$parent_dir/${basename_noext}${ext}"
            if [[ -f "$potential_output" ]]; then
                existing_files+=("$potential_output")
            fi
        done
        
        # Show warning if any existing files found
        if [[ ${#existing_files[@]} -gt 0 ]] && [[ "$mode" != "test" ]]; then
            echo -e "   \e[90mWarning: Existing output files will be overwritten:\e[0m"
            for existing_file in "${existing_files[@]}"; do
                echo -e "     \e[90m- $(basename "$existing_file")\e[0m"
            done
        fi
        
        echo -n "   Running: $PROG... "
        echo -en "${PROG_OUTPUT_COLOR}" # Set text color to highlight PROG output (light green)
        set +e  # Temporarily disable exit on error to handle ram failures gracefully
        if [[ "$mode" == "test" ]]; then
            { time "$PROG"; } >> "$LOG_FILE" 2>&1
            RETVAL=$?
        else
            echo
            "$PROG"
            RETVAL=$?
            echo -n "   "
        fi
        set -e  # Re-enable exit on error
        echo -en "\x1B[0m" # Reset text color
        # check PROG exit status
        if [[ $RETVAL -eq 0 ]]; then
            if [[ "$mode" != "test" ]]; then
                echo -n "$PROG "
            fi
            echo -e "\e[32mOK\e[0m"
            if [[ "$mode" != "test" ]]; then
                echo "   Success: ${PROG} completed successfully for $infile"
            fi
        else
            echo -e "\e[31mFAIL\e[0m"
            echo -e "   \e[31mError: ${PROG} failed with exit code $RETVAL for $infile\e[0m"
            echo "   Error: ${PROG} failed with exit code $RETVAL for $infile" >> "$LOG_FILE"
            echo "   Aborting..."
            break
        fi
        
        # After running the executable, move the output file to the target directory
        test="$directory/${basename_noext}.line"
        if [[ -f "$parent_dir/tl.line" ]]; then
            mv "$parent_dir/tl.line" "$test"
            echo "   Moved tl.line to $test"
        else
            echo -e "   \e[33mWarning: Expected output file tl.line not found in parent directory\e[0m"
        fi
    fi
    
    # For 'make' mode, we're done after running ram1.5.exe and moving output - skip file operations
    if [[ "$mode" == "make" ]]; then
        printf '%*s\n' "$line_len" '' | tr '  ' '='
        continue
    fi
    
    # Handle the output file
    test="$directory/${basename_noext}.line"
    ref="$directory/${basename_noext}.tl"
    
    # For make and test modes, check if test file exists (after moving from parent directory)
    # For copy mode, check if basename.line exists directly
    # For diff mode, check if either test or ref exists
    if [[ ("$mode" == "test") && -f "$parent_dir/tl.line" ]] || [[ "$mode" == "copy" && -f "$test" && -s "$test" ]] || [[ "$mode" == "diff" && (-f "$test" || -f "$ref") ]]; then
        # Rename tl.line to basename.line if it exists (for test mode only, since make mode already moved it)
        if [[ "$mode" == "test" ]]; then
            if [[ -f "$parent_dir/tl.line" ]]; then
                mv "$parent_dir/tl.line" "$test"
                echo "   Renamed tl.line to $test"
            fi
        fi
        
        # For diff mode, we need to check files even if test doesn't exist
        if [[ "$mode" == "diff" && ! -f "$test" ]]; then
            # In diff mode, check if ref exists
            if [[ -f "$ref" ]]; then
                echo -e "\e[33m[[MISSING OUTPUT]]\e[0m\n   Output file '$test' does not exist"
                echo -e "   \e[33mHint: Run 'make' mode first to generate output files\e[0m"
                # In diff mode, only mark as missing, not as failed (since no comparison was attempted)
                missing_output_files+=("$test")
            else
                echo -e "\e[33m[[MISSING FILES]]\e[0m\n   Both output '$test' and reference '$ref' do not exist"
                echo -e "   \e[33mHint: Run 'copy' mode first to generate reference files, then 'test' mode for output files\e[0m"
                # In diff mode, only mark as missing, not as failed (since no comparison was attempted)
                missing_output_files+=("$test")
                missing_reference_files+=("$ref")
            fi
            elif [[ -f "$test" && -s "$test" ]]; then
            if [[ "$mode" == "copy" ]]; then
                # COPY mode: Copy basename.line to .tl
                cp -v "$test" "$ref"
                echo "   Copied $test to $ref"
                processed_files+=("$infile")
                elif [[ "$mode" == "test" || "$mode" == "diff" ]]; then
                # TEST and DIFF modes: Compare files
                echo -n "   Comparing $test to $ref... "
                
                # Check if reference file exists
                if [[ ! -f "$ref" ]]; then
                    echo -e "\e[33m[[MISSING REFERENCE]]\e[0m\n   Reference file '$ref' does not exist"
                    if [[ "$mode" == "test" ]]; then
                        echo -e "\e[33m[[MISSING REFERENCE]]\e[0m\n   Reference file '$ref' does not exist" >> "$LOG_FILE"
                        fail_files+=("$infile")
                    fi
                    echo -e "   \e[33mHint: Run 'copy' mode first to generate reference files\e[0m"
                    missing_reference_files+=("$ref")
                    # Check if reference file is empty
                    elif [[ ! -s "$ref" ]]; then
                    echo
                    echo -e "\e[33m[[EMPTY REFERENCE]]\e[0m"
                    echo "   Reference file '$ref' is empty"
                    if [[ "$mode" == "test" ]]; then
                        echo -e "\e[33m[[EMPTY REFERENCE]]\e[0m"
                        echo "   Reference file '$ref' is empty" >> "$LOG_FILE"
                        fail_files+=("$infile")
                    fi
                    empty_files+=("$ref")
                else
                    # Perform the actual comparison
                    if [[ "$mode" == "test" ]]; then
                        # TEST mode: Log comparison results
                        if diff_files "$test" "$ref" >> "$LOG_FILE" 2>&1; then
                            echo -e "\e[32m[[PASS]]\e[0m" # Print PASS to terminal
                            echo -e "\e[32m[[PASS]]\e[0m" >> "$LOG_FILE"
                            pass_files+=("$infile")
                        else
                            echo -e "\e[31m[[FAIL]]\e[0m" # Print FAIL to terminal
                            echo -e "\e[31m[[FAIL]]\e[0m" >> "$LOG_FILE"
                            fail_files+=("$infile")
                        fi
                    else
                        # DIFF mode: Show comparison results to terminal only
                        if diff_files "$test" "$ref"; then
                            echo -e "$infile \e[32m[[PASS]]\e[0m" # Print PASS to terminal
                            pass_files+=("$infile")
                        else
                            echo -e "$infile \e[31m[[FAIL]]\e[0m" # Print FAIL to terminal
                            fail_files+=("$infile")
                        fi
                    fi
                fi
            fi
        fi
    else
        if [[ "$mode" == "copy" ]]; then
            echo -e "   \e[33m[[SKIPPED]]\e[0m No $test file found to rename"
            skipped_files+=("$infile")
            elif [[ "$mode" == "test" ]]; then
            echo -e "   \e[36m[[INFO]]\e[0m Output file tl.line will be generated by ram1.5.exe in parent directory"
        else
            echo -e "\e[33m[[NO FILES]]\e[0m"
            echo "No output file (basename.line) or reference file (.tl) found for $basename_noext"
            if [[ "$mode" == "diff" ]]; then
                echo -e "   \e[33mHint: Run 'make' mode to generate output files, and 'copy' mode to generate reference files\e[0m"
            else
                fail_files+=("$infile")
            fi
        fi
    fi
    
    # Clean up extra files
    for f in "$directory/$basename_noext"*; do
        # Skip .in, reference, output, and log
        if [[ "$f" == "$infile" || "$f" == "$ref" || "$f" == "$test" || "$f" == "$LOG_FILE" ]]; then
            continue
        fi
        # Only delete files
        if [[ -f "$f" ]]; then
            rm "$f"
            if [[ "$mode" != "test" ]]; then
                echo "   Deleted $f"
            fi
        fi
    done
    
    # Clean up ram.in from parent directory if it exists
    if [[ -f "$parent_dir/ram.in" ]]; then
        rm "$parent_dir/ram.in"
        if [[ "$mode" != "test" ]]; then
            echo "   Deleted $parent_dir/ram.in"
        fi
    fi
    printf '\n%*s\n' "$line_len" '' | tr ' ' '='
done

# Print summary if in test, diff, or copy mode
if [[ "$mode" == "test" || "$mode" == "diff" || "$mode" == "copy" ]]; then
    if [[ "$mode" == "test" ]]; then
        echo "Test Summary:"
        elif [[ "$mode" == "copy" ]]; then
        echo "Copy Summary:"
    else
        echo "Diff Summary:"
    fi
    printf '%*s\n' "$line_len" '' | tr ' ' '='
    if [[ "$mode" == "copy" ]]; then
        echo "Processed files: ${#processed_files[@]}"
        for f in "${processed_files[@]}"; do
            echo -e "   \e[32mCOPIED\e[0m $f"
        done
        echo "Skipped files: ${#skipped_files[@]}"
        for f in "${skipped_files[@]}"; do
            echo -e "   \e[33mSKIPPED\e[0m $f"
        done
    else
        echo "Passed files: ${#pass_files[@]}"
        for f in "${pass_files[@]}"; do
            echo -e "   \e[32mPASS\e[0m $f"
        done
        echo "Failed files: ${#fail_files[@]}"
        for f in "${fail_files[@]}"; do
            echo -e "   \e[31mFAIL\e[0m $f"
        done
    fi
    
    # Show detailed missing file information
    if [[ ${#missing_reference_files[@]} -gt 0 ]]; then
        echo -e "\nMissing reference files (run 'copy' mode first):"
        for f in "${missing_reference_files[@]}"; do
            echo -e "   \e[33mMISSING\e[0m $f"
        done
    fi
    
    if [[ ${#missing_output_files[@]} -gt 0 ]]; then
        echo -e "\nMissing output files (run 'make' mode first):"
        for f in "${missing_output_files[@]}"; do
            echo -e "   \e[33mMISSING\e[0m $f"
        done
    fi
    
    if [[ ${#empty_files[@]} -gt 0 ]]; then
        echo -e "\nEmpty files detected:"
        for f in "${empty_files[@]}"; do
            echo -e "   \e[33mEMPTY\e[0m $f"
        done
    fi
    
    if [[ "$mode" == "copy" && ${#skipped_files[@]} -eq 0 ]]; then
        echo -e "\nAll files processed!"
        elif [[ "$mode" != "copy" && ${#fail_files[@]} -eq 0 ]]; then
        echo -e "\nAll files passed!"
        elif [[ ${#missing_reference_files[@]} -gt 0 || ${#missing_output_files[@]} -gt 0 ]]; then
        echo -e "\n\e[33mSome failures are due to missing files. Consider running the suggested modes first.\e[0m"
    fi
    echo "=============================="
fi
