#!/bin/bash
# Usage: ./process_in_files.sh <mode> [directory]
# Aug 2025 JCL
# =============================
# EXPLANATION SECTION
#
# This script processes .in files in a directory using nspe.exe.
#
# Input options (first argument):
#   make - Runs nspe.exe only (no file operations)
#   copy - Runs nspe.exe and renames output files to standard names (.tl, .rtl, .ftl)
#   test - Runs nspe.exe, logs output, and compares results to reference files
#   diff - Compares existing output files to reference files (does not run nspe.exe)
#
# PROMPT: To process files, run:
#   ./process_in_files.sh <mode> [directory]
# Where <mode> is 'make', 'copy', 'test', or 'diff'. Directory defaults to 'std'.
#
# Dependencies:
#   - lib_diff_utils.sh (must be in same directory or PATH)
#   - nspe.exe
#   - Optional: tldiff, uband_diff for advanced comparisons
#
# Replaces functionality of:
#
#   script             | mode
#   -------------------+------
#   std/copy_std.bat   | COPY
#   std/mkstd          | MAKE
#   std/testram        | TEST
#   std/testram_getarg | TEST
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
#   - Runs nspe.exe on each .in file (generates output files)
#   - Does not perform any file operations (move, copy, diff)
#   - Use when you just want to generate output files
#
# 'copy':
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
    echo "  make - Run NSPE only (no file operations) [mkstd]"
    echo "  copy - Run NSPE and rename output files to reference files [copy_std.bat]"
    echo "  test - Time and run NSPE and compare outputs to reference files [testram or testram_getarg]"
    echo "  diff - Compare existing output files to reference files (no NSPE execution)"
    exit 1
fi

if [[ ! -d "$directory" ]]; then
    echo "Error: Directory '$directory' does not exist. Exiting."
    exit 1
fi

# Intelligent program detection - handles nspe.x, nspe.exe, or makefile targets
PROG_OUTPUT_COLOR="\x1B[38;5;71m" # Light green color for PROG output

detect_nspe_program() {
    local candidates=("./nspe.x" "./nspe.exe" "nspe.x" "nspe.exe")
    local makefile_targets=()
    
    # Check for existing executables first
    for prog in "${candidates[@]}"; do
        if [[ -f "$prog" && -x "$prog" ]]; then
            echo "Found existing executable: $prog" >&2
            echo "$prog"
            return 0
        fi
    done
    
    # If no executable found, check makefile for available targets
    if [[ -f "makefile" ]] || [[ -f "Makefile" ]]; then
        echo "No executable found. Checking makefile for nspe targets..." >&2
        
        # Look for nspe targets in makefile (case insensitive)
        if command -v make >/dev/null 2>&1; then
            # Try to get makefile targets (this works with GNU make)
            local make_targets
            make_targets=$(make -qp 2>/dev/null | grep -E '^[^.%#[:space:]][^=]*:' | cut -d: -f1 | grep -i nspe || true)
            
            if [[ -n "$make_targets" ]]; then
                echo "Found makefile targets containing 'nspe':" >&2
                echo "$make_targets" | sed 's/^/  /' >&2
                
                # Prefer .x over .exe, then first match
                for target in nspe.x nspe.exe $make_targets; do
                    if echo "$make_targets" | grep -q "^$target$"; then
                        echo "Selected target: $target" >&2
                        if [[ "$target" == nspe.* ]]; then
                            echo "./$target"
                        else
                            echo "$target"
                        fi
                        return 0
                    fi
                done
                
                # If no exact match, use first target found
                local first_target
                first_target=$(echo "$make_targets" | head -n1)
                echo "Using first available target: $first_target" >&2
                if [[ "$first_target" == nspe.* ]]; then
                    echo "./$first_target"
                else
                    echo "$first_target"
                fi
                return 0
            fi
        fi
    fi
    
    # Default fallback
    echo "No executable or makefile target found. Using default: ./nspe.x" >&2
    echo "./nspe.x"
    return 0
}

# Detect the appropriate nspe program
PROG=$(detect_nspe_program)
echo "Selected program: $PROG"

# Check if the program exists, and build it if not
if [[ ! -f "$PROG" ]]; then
    echo "Program $PROG not found. Attempting to build it..."
    if command -v make >/dev/null 2>&1; then
        # Extract just the target name for make (remove ./ prefix)
        make_target="${PROG#./}"
        echo "Running: make $make_target"
        echo -en "${PROG_OUTPUT_COLOR}"
        if make "$make_target"; then
            echo -en "\x1B[0m" # Reset text color
            echo "Successfully built $PROG"
        else
            echo -e "\e[31mError: Failed to build $PROG. Please check your makefile and dependencies.\e[0m"
            exit 1
        fi
    else
        echo -e "\e[31mError: make command not found and $PROG does not exist.\e[0m"
        echo "Please build $PROG manually or install make."
        exit 1
    fi
fi

# Verify the program is now executable
if [[ ! -x "$PROG" ]]; then
    echo -e "\e[31mError: $PROG exists but is not executable.\e[0m"
    echo "Making it executable..."
    chmod +x "$PROG"
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

# Print mode header once at the beginning
term_width=$(tput cols 2>/dev/null || echo 80)
line_len=$((term_width * 5 / 10))

printf '%*s\n' "$line_len" '' | tr '  ' '='
echo "Processing directory: $directory"
echo "Mode: $mode"
if [[ "$mode" == "make" ]]; then
    echo "Will run nspe.exe only (no file operations)"
    elif [[ "$mode" == "copy" ]]; then
    echo "Will run nspe.exe and rename output files to reference files (.tl, .rtl, .ftl)"
    elif [[ "$mode" == "test" ]]; then
    echo "Will run nspe.exe and compare outputs to reference files"
    elif [[ "$mode" == "diff" ]]; then
    echo "Will compare existing output files to reference files (no nspe.exe execution)"
fi
printf '%*s\n' "$line_len" '' | tr '  ' '='

for infile in "${infiles[@]}"; do
    # Check for required strings (case-insensitive)
    if grep -qi '^tl' "$infile" || grep -qi '^rtl' "$infile" || grep -Eiq '^hrfa|^hfra|^hari' "$infile"; then
        echo "Processing: $infile"
        LOG_FILE="$directory/$(basename "$infile" .in).log"
        
        # Run nspe.exe for 'make', 'copy' and 'test' modes, but not for 'diff' mode
        if [[ "$mode" == "make" || "$mode" == "copy" || "$mode" == "test" ]]; then
            # Check for existing output files and warn user
            basename_noext="$(basename "$infile" .in)"
            existing_files=()
            for suffix in 01 02 03; do
                potential_output="$directory/${basename_noext}_${suffix}.asc"
                if [[ -f "$potential_output" ]]; then
                    existing_files+=("$potential_output")
                fi
            done
            
            # Also check for other common output files
            for ext in .003 _41.dat _42.dat .log; do
                potential_output="$directory/${basename_noext}${ext}"
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
            
            echo -n "   Running: $PROG $infile... "
            echo -en "${PROG_OUTPUT_COLOR}" # Set text color to highlight PROG output (light green)
            set +e  # Temporarily disable exit on error to handle nspe failures gracefully
            if [[ "$mode" == "test" ]]; then
                { time "$PROG" "$infile"; } >> "$LOG_FILE" 2>&1
                RETVAL=$?
            else
                echo
                "$PROG" "$infile"
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
                    echo "   Success: nspe.exe completed successfully for $infile"
                fi
            else
                echo -e "\e[31mFAIL\e[0m"
                echo -e "   \e[31mError: nspe.exe failed with exit code $RETVAL for $infile\e[0m"
                echo "   Error: nspe.exe failed with exit code $RETVAL for $infile" >> "$LOG_FILE"
                echo "   Aborting..."
                break
                #continue
            fi
        fi
        
        # For 'make' mode, we're done after running nspe.exe - skip file operations
        if [[ "$mode" == "make" ]]; then
            printf '%*s\n' "$line_len" '' | tr '  ' '='
            continue
        fi
        
        # Find the first available file pair by priority: 03, 02, 01
        found_files=false
        for suffix in 03 02 01; do
            test="$directory/$(basename "$infile" .in)_${suffix}.asc"
            case $suffix in
                01) ref="$directory/$(basename "$infile" .in).tl" ;;
                02) ref="$directory/$(basename "$infile" .in).rtl" ;;
                03) ref="$directory/$(basename "$infile" .in).ftl" ;;
            esac
            
            # For copy and test modes, check if test file exists
            # For diff mode, check if either test or ref exists to provide meaningful feedback
            if [[ ("$mode" == "copy" || "$mode" == "test") && -f "$test" && -s "$test" ]] || [[ "$mode" == "diff" && (-f "$test" || -f "$ref") ]]; then
                # For diff mode, we need to check files even if test doesn't exist
                if [[ "$mode" == "diff" && ! -f "$test" ]]; then
                    # In diff mode, check if either test or ref files exist to provide meaningful feedback
                    if [[ -f "$ref" ]]; then
                        echo -e "\e[33m[[MISSING OUTPUT]]\e[0m\n   Output file '$test' does not exist"
                        echo -e "   \e[33mHint: Run 'make' mode first to generate output files\e[0m"
                        fail_files+=("$infile")
                        missing_output_files+=("$test")
                        found_files=true
                    else
                        echo -e "\e[33m[[MISSING FILES]]\e[0m\n   Both output '$test' and reference '$ref' do not exist"
                        echo -e "   \e[33mHint: Run 'copy' mode first to generate reference files, then 'test' mode for output files\e[0m"
                        fail_files+=("$infile")
                        missing_output_files+=("$test")
                        missing_reference_files+=("$ref")
                        found_files=true
                    fi
                    break  # Exit the suffix loop since we handled this case
                fi
                
                # Continue with existing logic for cases where test file exists
                if [[ -f "$test" && -s "$test" ]]; then
                    if [[ "$mode" == "copy" ]]; then
                        # COPY mode: Only rename files, no comparison
                        mv "$test" "$ref"
                        echo "   Renamed $test to $ref"
                        elif [[ "$mode" == "test" || "$mode" == "diff" ]]; then
                        # TEST and DIFF modes: Compare files
                        echo -n "   Comparing $test to $ref... "
                        
                        # Check if reference file exists
                        if [[ ! -f "$ref" ]]; then
                            echo -e "\e[33m[[MISSING REFERENCE]]\e[0m\n   Reference file '$ref' does not exist"
                            if [[ "$mode" == "test" ]]; then
                                echo -e "\e[33m[[MISSING REFERENCE]]\e[0m\n   Reference file '$ref' does not exist" >> "$LOG_FILE"
                            fi
                            echo -e "   \e[33mHint: Run 'copy' mode first to generate reference files\e[0m"
                            fail_files+=("$infile")
                            missing_reference_files+=("$ref")
                            found_files=true  # Mark as processed to avoid "no files found" message
                            break  # Exit the suffix loop since we handled this case
                        fi
                        
                        # Check if reference file is empty
                        if [[ ! -s "$ref" ]]; then
                            echo
                            echo -e "\e[33m[[EMPTY REFERENCE]]\e[0m"
                            echo "   Reference file '$ref' is empty"
                            if [[ "$mode" == "test" ]]; then
                                echo -e "\e[33m[[EMPTY REFERENCE]]\e[0m"
                                echo "   Reference file '$ref' is empty" >> "$LOG_FILE"
                            fi
                            fail_files+=("$infile")
                            empty_files+=("$ref")
                            found_files=true  # Mark as processed to avoid "no files found" message
                            break  # Exit the suffix loop since we handled this case
                        fi
                        
                        # For diff mode, also check if test file exists (since we're not running nspe.exe)
                        if [[ "$mode" == "diff" && ! -f "$test" ]]; then
                            echo -e "\e[33m[[MISSING OUTPUT]]\e[0m\n   Output file '$test' does not exist"
                            echo -e "   \e[33mHint: Run 'test' mode first to generate output files\e[0m"
                            fail_files+=("$infile")
                            missing_output_files+=("$test")
                            continue
                        fi
                        
                        # For diff mode, check if test file is empty
                        if [[ "$mode" == "diff" && ! -s "$test" ]]; then
                            echo -e "\e[33m[[EMPTY OUTPUT]]\e[0m\n   Output file '$test' is empty"
                            fail_files+=("$infile")
                            empty_files+=("$test")
                            continue
                        fi
                        
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
                    else
                        echo "Unknown mode: $mode"
                        exit 1
                    fi
                    # After renaming/diffing, delete files with same base name except .in and selected reference
                    basename_noext="$(basename "$infile" .in)"
                    for f in "$directory/$basename_noext"*; do
                        # Skip .in, selected reference, and test file
                        if [[ "$f" == "$infile" || "$f" == "$ref" || "$f" == "$test" ]]; then
                            continue
                        fi
                        # Skip log file in test mode
                        if [[ "$f" == "$LOG_FILE" && "$mode" == "test" ]]; then
                            continue
                        fi
                        # Skip if test file contains "case7" or "case6"
                        if [[ "$f" == "$LOG_FILE" ]] && [[ "$test" == *case7* || "$test" == *case6* ]]; then
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
                    found_files=true
                    break
                fi  # End of the "if [[ -f "$test" && -s "$test" ]]; then" block
            fi  # End of the main suffix condition
        done
        
        # If no files were found, report it
        if [[ "$found_files" == false ]]; then
            if [[ "$mode" == "copy" ]]; then
                echo -e "   \e[36m[[INFO]]\e[0m Output files will be generated by nspe.exe for suffixes (01, 02, 03)"
            else
                echo -e "\e[33m[[NO FILES]]\e[0m"
                echo "No output files (01, 02, 03) or reference files (tl, rtl, ftl) found for $basename_noext"
                if [[ "$mode" == "diff" ]]; then
                    echo -e "   \e[33mHint: Run 'make' mode to generate output files, and 'copy' mode to generate reference files\e[0m"
                fi
                fail_files+=("$infile")
            fi
        fi
    else
        echo -e "\e[33mSkipping $infile (does not contain required strings)\e[0m"
    fi
    printf '\n%*s\n' "$line_len" '' | tr ' ' '='
done

# Print summary if in test or diff mode
if [[ "$mode" == "test" || "$mode" == "diff" ]]; then
    #printf '%*s\n' "$line_len" '' | tr ' ' '='
    if [[ "$mode" == "test" ]]; then
        echo "Test Summary:"
    else
        echo "Diff Summary:"
    fi
    printf '%*s\n' "$line_len" '' | tr ' ' '='
    echo "Passed files: ${#pass_files[@]}"
    for f in "${pass_files[@]}"; do
        echo -e "   \e[32mPASS\e[0m $f"
    done
    echo "Failed files: ${#fail_files[@]}"
    for f in "${fail_files[@]}"; do
        echo -e "   \e[31mFAIL\e[0m $f"
    done
    
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
    
    if [[ ${#fail_files[@]} -eq 0 ]]; then
        echo -e "\nAll files passed!"
        elif [[ ${#missing_reference_files[@]} -gt 0 || ${#missing_output_files[@]} -gt 0 ]]; then
        echo -e "\n\e[33mSome failures are due to missing files. Consider running the suggested modes first.\e[0m"
    fi
    echo "=============================="
fi
